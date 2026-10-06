"""Apply the evolution budgets to the existing nine-round measurement."""
import ctypes
import json
from pathlib import Path
import sys
import os
import subprocess


root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools'))
import run_playable_performance as measurement
from working_set_budget import working_set_growth

class Counters(ctypes.Structure):
    _fields_ = [('cb', ctypes.c_ulong), ('faults', ctypes.c_ulong)] + [
        (name, ctypes.c_size_t) for name in ('peak', 'working', 'quotaPeakPaged', 'quotaPaged',
                                           'quotaPeakNonPaged', 'quotaNonPaged', 'pagefile', 'peakPagefile')]

kernel = ctypes.WinDLL('kernel32', use_last_error=True)
kernel.OpenProcess.restype = ctypes.c_void_p
kernel.CloseHandle.argtypes = [ctypes.c_void_p]
psapi = ctypes.WinDLL('psapi')
psapi.GetProcessMemoryInfo.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_ulong]
current_samples = []

def memory(pid):
    handle = kernel.OpenProcess(0x410, False, pid)
    if not handle:
        return 0
    try:
        data = Counters()
        data.cb = ctypes.sizeof(data)
        if not psapi.GetProcessMemoryInfo(handle, ctypes.byref(data), data.cb):
            return 0
        if data.working:
            current_samples.append(data.working)
        return data.peak
    finally:
        kernel.CloseHandle(handle)

original_run = measurement.run

def run(*args, **kwargs):
    current_samples.clear()
    editor = args[3] if len(args)>3 else kwargs.get('editor',False)
    config = None
    saved = os.environ.get('AZURERENDER_EDITOR_CONFIG')
    if editor:
        output = args[2]
        config = output.parent / 'editor-config' / output.name
        config.mkdir(parents=True,exist_ok=False)
        os.environ['AZURERENDER_EDITOR_CONFIG'] = str(config.resolve())
    try:
        result = original_run(*args, **kwargs)
    finally:
        if saved is None: os.environ.pop('AZURERENDER_EDITOR_CONFIG',None)
        else: os.environ['AZURERENDER_EDITOR_CONFIG']=saved
    if editor:
        workspace = json.loads((config/'workspace.json').read_text(encoding='utf-8'))
        assert set(workspace['panels']) == {'viewport','outliner','inspector','assets','capture','console','build','animation','gameplay-debug','settings'}
        assert all(workspace['panels'][name] for name in ('viewport','outliner','inspector','assets','capture','console','build','animation','gameplay-debug'))
        assert workspace['panels']['settings'] is False
        result['workspace']={'before':'fresh configuration; default panels enabled','after':workspace,'layout':(config/'layout.ini').read_text(encoding='utf-8')}
        result['windowExtent']=[1920,1080]
    stable = current_samples[len(current_samples) // 2:]
    assert len(stable) >= 5, 'Insufficient working-set observations'
    result['workingSetSamplesBytes'] = list(current_samples)
    result['stableWorkingSetGrowth'] = working_set_growth(current_samples)
    result['workingSetObservationIntervalMs'] = 50
    return result

measurement.memory = memory
measurement.run = run
sys.argv = [str(root / 'tools/run_playable_performance.py'),
    '--player', str(root / 'build/ninja-msvc-release/AzurePlayer.exe'),
    '--editor', str(root / 'build/ninja-msvc-release/AzureRender.exe'),
    '--output', str(root / 'build/evolution/r7/performance')]
assert measurement.main() == 0
folder = root / 'build/evolution/r7/performance'
summary = json.loads((folder / 'summary.json').read_text())
checks = []
for load in ('standard', 'editor', 'stress'):
    assert len(summary['rounds'][load]) == 3
    for index, result in enumerate(summary['rounds'][load]):
        cpu_limit = 10 if load == 'editor' else 8
        passed = (result['gpu']['p95'] <= 16.6 and result['cpu']['p95'] <= cpu_limit
            and result['physics']['p95'] <= 2 and result['stableWorkingSetGrowth'] <= .05
            and 0 < result['peakWorkingSetBytes'] <= 2 * 1024**3
            and result['allocator']['deviceLocalPeakBytes'] <= 1.5 * 1024**3
            and result['samples'] == 1800 and result['warmup'] == 300)
        checks.append({'load': load, 'round': index + 1, 'passed': passed,
            'gpuP95': result['gpu']['p95'], 'cpuP95': result['cpu']['p95'],
            'physicsP95': result['physics']['p95'], 'workingSetGrowth': result['stableWorkingSetGrowth']})
report = {'schemaVersion': 1, 'passed': all(row['passed'] for row in checks),
    'gpuLimitMs': 16.6, 'playerCpuLimitMs': 8, 'editorCpuLimitMs': 10,
    'memoryObservation': 'positive peak growth from the first observation in the latter half of process observations',
    'checks': checks}
(folder / 'strict-budget.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report), flush=True)
assert report['passed'], 'Evolution performance budgets exceeded'
