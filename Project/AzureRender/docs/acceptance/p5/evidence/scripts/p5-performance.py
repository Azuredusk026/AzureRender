"""Run the frozen P5 workloads under the unchanged formal budgets."""
import ctypes
import hashlib
import json
import os
from pathlib import Path
import shutil
import statistics
import sys

ROOT = Path.cwd().resolve()
sys.path.insert(0, str(ROOT / 'tools'))
import run_playable_performance as measurement
from build_provenance import inputs, verify

OUT = ROOT / 'build/implementation-2026-10-10/p5-performance'
OUT.mkdir(parents=True, exist_ok=False)
for config in ('debug', 'release'):
    verify(ROOT, json.loads((ROOT / f'build/ninja-msvc-{config}/build-provenance.json').read_text(encoding='utf-8')))
frozen = inputs(ROOT)
products = {name: hashlib.sha256((ROOT / 'build/ninja-msvc-release' / name).read_bytes()).hexdigest()
            for name in ('AzureRender.exe', 'AzurePlayer.exe')}

class Counters(ctypes.Structure):
    _fields_ = [('cb', ctypes.c_ulong), ('faults', ctypes.c_ulong)] + [
        (name, ctypes.c_size_t) for name in ('peak', 'working', 'quotaPeakPaged', 'quotaPaged',
            'quotaPeakNonPaged', 'quotaNonPaged', 'pagefile', 'peakPagefile')]
kernel = ctypes.WinDLL('kernel32', use_last_error=True)
kernel.OpenProcess.restype = ctypes.c_void_p
kernel.CloseHandle.argtypes = [ctypes.c_void_p]
psapi = ctypes.WinDLL('psapi')
psapi.GetProcessMemoryInfo.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_ulong]
observations = []
def memory(pid):
    handle = kernel.OpenProcess(0x410, False, pid)
    if not handle:
        return 0
    try:
        data = Counters(); data.cb = ctypes.sizeof(data)
        if not psapi.GetProcessMemoryInfo(handle, ctypes.byref(data), data.cb):
            return 0
        if data.working:
            observations.append(data.working)
        return data.peak
    finally:
        kernel.CloseHandle(handle)
measurement.memory = memory

rounds = {}; quality = {}; content = {}
for name, stress, editor in [('standard', False, False), ('editor', False, True), ('stress', True, False),
                            ('exploration', False, False), ('platformer', False, False)]:
    folder = OUT / name
    if name in ('exploration', 'platformer'):
        shutil.copytree(ROOT / 'assets_public' / name, folder, ignore=shutil.ignore_patterns('.azure'))
        quality[name] = json.loads((folder / 'QUALITY.json').read_text(encoding='utf-8'))
    else:
        quality[name] = measurement.fixture(folder, stress)
    content[name] = {p.relative_to(folder).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                     for p in sorted(folder.rglob('*')) if p.is_file() and '.azure' not in p.parts}
    rounds[name] = []
    for index in range(3):
        observations.clear()
        if editor:
            os.environ['AZURERENDER_EDITOR_CONFIG'] = str(OUT / f'editor-config-{index+1}')
        else:
            os.environ.pop('AZURERENDER_EDITOR_CONFIG', None)
        result = measurement.run(ROOT / 'build/ninja-msvc-release' / ('AzureRender.exe' if editor else 'AzurePlayer.exe'),
                                 folder / 'project.azureproject', OUT / f'{name}-{index+1}', editor, stress)
        assert len(observations) >= 10 and all(v > 0 for v in observations)
        stable = observations[len(observations)//2:]
        result['workingSetSamplesBytes'] = list(observations)
        result['stableWorkingSetGrowth'] = max(stable) / stable[0] - 1
        result['workingSetObservationIntervalMs'] = 50
        result['passed'] = (result['gpu']['p95'] <= 16.6 and result['cpu']['p95'] <= (10 if editor else 8)
            and result['physics']['p95'] <= 2 and result['stableWorkingSetGrowth'] <= .05
            and 0 < result['peakWorkingSetBytes'] <= 2 * 1024**3
            and result['allocator']['deviceLocalPeakBytes'] <= 1.5 * 1024**3
            and result['samples'] == 1800 and result['warmup'] == 300)
        if editor:
            workspace = json.loads((Path(os.environ['AZURERENDER_EDITOR_CONFIG']) / 'workspace.json').read_text(encoding='utf-8'))
            assert all(workspace['panels'][p] for p in ('viewport', 'outliner', 'inspector', 'assets'))
            result['workspace'] = workspace
        rounds[name].append(result)
        (OUT / 'partial.json').write_text(json.dumps(rounds, indent=2) + '\n', encoding='utf-8')
        print(name, index+1, 'strict budget', result['passed'], flush=True)
assert inputs(ROOT) == frozen, 'Source inputs changed during performance measurement'
summary = dict(schemaVersion=1, products=products, sourceHash=hashlib.sha256(json.dumps(frozen, sort_keys=True).encode()).hexdigest(),
    quality=quality, content=content, rounds=rounds,
    budgets=dict(gpuP95Ms=16.6, playerCpuP95Ms=8, editorCpuP95Ms=10, physicsP95Ms=2,
        stableWorkingSetGrowth=.05, peakWorkingSetBytes=2*1024**3, deviceLocalBytes=1.5*1024**3),
    median={name: {key: statistics.median(r[key]['p95'] for r in rows) for key in ('gpu','cpu','physics')}
            for name, rows in rounds.items()},
    passed=all(r['passed'] for rows in rounds.values() for r in rows))
(OUT / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
print(json.dumps(dict(passed=summary['passed'], median=summary['median'])), flush=True)
assert summary['passed'], 'P5 formal budgets exceeded'
