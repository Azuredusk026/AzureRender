"""Compare an explicit Vulkan cache in owned children without the NT ring buffer."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

root = Path.cwd().resolve()
work = root/'build/p5-p6-closure-2026-10-10'
sys.path.insert(0, str(root/'tools'))
sys.path.insert(0, str(work/'native-memory-trace'))
import inject_owned as injection
from build_provenance import inputs, verify
from run_playable_performance import run

out = work/'cache-control-experiment'
out.mkdir(exist_ok=False)
exe = root/'build/ninja-msvc-release/AzurePlayer.exe'
dll = work/'native-memory-trace/compiled-ninja/AzureCacheShim.dll'
cache = out/'pipeline-cache.bin'
project = work/'final-formal-performance/exploration/project.azureproject'
proof = json.loads((exe.parent/'build-provenance.json').read_text(encoding='utf-8'))
verify(root, proof)
frozen = inputs(root)
baseline_input = json.loads((work/'final-formal-performance/exploration-3.input.json').read_text())
BasePopen = subprocess.Popen
mode = None

class OwnedCachePopen(BasePopen):
    def __init__(self, arguments, **keywords):
        selected = Path(arguments[0]).resolve() == exe.resolve()
        if not selected:
            super().__init__(arguments, **keywords)
            return
        keywords['creationflags'] = keywords.get('creationflags', 0) | 4
        super().__init__(arguments, **keywords)
        process = primary = None
        try:
            process = injection.checked(injection.OpenProcess(0x1fffff, False, self.pid))
            threads = injection.ownedThreads(self.pid)
            assert len(threads) == 1
            primary = injection.checked(injection.OpenThread(0x1fffff, False, threads[0]))
            injection.invoke(process, injection.GetProcAddress(injection.GetModuleHandleW('kernel32.dll'), b'LoadLibraryW'),
                             injection.remoteString(process, dll))
            remote = next(row for row in injection.modules(process) if Path(row['path']).resolve() == dll.resolve())
            local = injection.checked(injection.LoadLibraryW(str(dll.resolve())))
            entry = injection.checked(injection.GetProcAddress(local, b'AzureCacheStart'))
            parameter = ('disabled:' if mode == 'disabled' else '') + str(cache.resolve())
            result = injection.invoke(process, remote['base']+entry-local, injection.remoteString(process, parameter))
            assert result == 0, ('Cache diagnostic initialization failed', result)
            assert injection.ResumeThread(primary) == 1
        except BaseException:
            self.kill()
            self.wait(timeout=10)
            raise
        finally:
            if primary:
                injection.CloseHandle(primary)
            if process:
                injection.CloseHandle(process)

rows = []
subprocess.Popen = OwnedCachePopen
try:
    for case in ('disabled-1', 'disabled-2', 'disabled-3', 'cold', 'warm-1', 'warm-2', 'warm-3'):
        mode = 'disabled' if case.startswith('disabled') else 'enabled'
        initial = hashlib.sha256(cache.read_bytes()).hexdigest() if cache.exists() else None
        assert case != 'cold' or initial is None
        result = run(exe, project, out/case)
        assert json.loads((out/(case+'.input.json')).read_text()) == baseline_input
        text = (out/(case+'.stderr.log')).read_text(encoding='utf-8')
        matched = re.search(r'AZURE_DIAGNOSTIC_CACHE graphics=(\d+) compute=(\d+) loaded=(\d+) saved=(\d+) failed=(\d+)', text)
        assert matched, 'The diagnostic must confirm actual pipeline interception'
        counts = dict(zip(('graphicsCalls', 'computeCalls', 'loadedBytes', 'savedBytes', 'failedCaches'),
                          map(int, matched.groups())))
        assert counts['graphicsCalls'] > 0 and counts['computeCalls'] > 0 and counts['failedCaches'] == 0
        if case.startswith('warm'):
            assert counts['loadedBytes'] > 0 and initial
        memory = json.loads((out/(case+'.memory.json')).read_text())
        stable = memory[len(memory)//2:]
        rows.append(dict(case=case, diagnosticOnly=True, formalAcceptance=False, result=result,
                         cache=counts, initialCacheSha256=initial,
                         finalCacheSha256=hashlib.sha256(cache.read_bytes()).hexdigest() if cache.exists() else None,
                         stableWorkingSetIncreaseBytes=max(row['working'] for row in stable)-stable[0]['working']))
        assert inputs(root) == frozen
        (out/'partial.json').write_text(json.dumps(rows, indent=2)+'\n')
        print(case, counts, 'absolute increase', rows[-1]['stableWorkingSetIncreaseBytes'], flush=True)
finally:
    subprocess.Popen = BasePopen
verify(root, proof)
summary = dict(diagnosticOnly=True, formalAcceptance=False, source=proof['source'], baseCommit=proof['commit'],
               binarySha256=hashlib.sha256(exe.read_bytes()).hexdigest(),
               shimSha256=hashlib.sha256(dll.read_bytes()).hexdigest(),
               measurements=rows, rootCauseFixed=False,
               limits=['The shim changes startup and cache dispatch; these are experimental measurements.',
                       'No process-local NT ring buffer is loaded.',
                       'Warm cache results require allocation-event comparison and production validation.'])
(out/'summary.json').write_text(json.dumps(summary, indent=2)+'\n')
print('Cache experiment measurements retained; production acceptance remains separate', flush=True)
