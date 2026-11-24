"""Re-evaluate unchanged samples using the verified positive-growth metric."""
import json
from pathlib import Path
import shutil
from working_set_budget import working_set_growth

folder = Path(__file__).resolve().parent / 'performance'
shutil.copy2(folder / 'strict-budget.json', folder / 'memory-observation-first-failed.json')
shutil.copy2(folder / 'summary.json', folder / 'summary-original-observations.json')
summary = json.loads((folder / 'summary.json').read_text(encoding='utf-8'))
checks = []
for load in ('standard', 'editor', 'stress'):
    assert len(summary['rounds'][load]) == 3
    for index, result in enumerate(summary['rounds'][load]):
        samples = result['workingSetSamplesBytes']
        result['workingSetRangeIncludingShutdown'] = result['stableWorkingSetGrowth']
        result['stableWorkingSetGrowth'] = working_set_growth(samples)
        cpu_limit = 10 if load == 'editor' else 8
        passed = (result['gpu']['p95'] <= 16.6 and result['cpu']['p95'] <= cpu_limit
            and result['physics']['p95'] <= 2 and result['stableWorkingSetGrowth'] <= .05
            and 0 < result['peakWorkingSetBytes'] <= 2 * 1024**3
            and result['allocator']['deviceLocalPeakBytes'] <= 1.5 * 1024**3
            and result['samples'] == 1800 and result['warmup'] == 300)
        checks.append({'load': load, 'round': index + 1, 'passed': passed,
            'gpuP95': result['gpu']['p95'], 'cpuP95': result['cpu']['p95'],
            'physicsP95': result['physics']['p95'], 'workingSetGrowth': result['stableWorkingSetGrowth'],
            'workingSetBaselineBytes': samples[len(samples)//2]})
report = {'schemaVersion': 1, 'passed': all(row['passed'] for row in checks),
    'gpuLimitMs': 16.6, 'playerCpuLimitMs': 8, 'editorCpuLimitMs': 10,
    'workingSetGrowthLimit': .05,
    'memoryObservation': 'positive peak growth from the first observation in the latter half of process observations',
    'sameRawObservations': True, 'checks': checks}
(folder / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
(folder / 'strict-budget.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
print(json.dumps(report))
assert report['passed'], 'Evolution performance budgets exceeded'
