"""Run original gates serially; retain one environment timeout per batch."""
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path.cwd()
OUT = ROOT / 'build/p8-format-closure-2026-10-12'
sys.path.insert(0, str(ROOT / 'tools'))
import run_playable_performance as performance
from gpu_environment import wait_and_record
from build_game import build_game

gate_failure = None
def bounded_gate(path, **options):
    global gate_failure
    if gate_failure is not None:
        path.write_text(json.dumps(dict(status='NotExecutedAfterEnvironmentTimeout',
            originalEvidence=str(gate_failure)), indent=2), encoding='utf-8')
        raise TimeoutError('Batch environment gate failed; see ' + str(gate_failure))
    try:
        return wait_and_record(path, **options)
    except TimeoutError:
        gate_failure = path
        raise

performance.wait_and_record = bounded_gate
sys.argv = ['run_playable_performance.py', '--player', str(ROOT/'build/ninja-msvc-release/AzurePlayer.exe'),
    '--editor', str(ROOT/'build/ninja-msvc-release/AzureRender.exe'),
    '--exploration', str(ROOT/'assets_public/exploration/project.azureproject'),
    '--platformer', str(ROOT/'assets_public/platformer/project.azureproject'),
    '--output', str(OUT/'final-formal-performance')]
status = performance.main()
print('Formal performance exit:', status, flush=True)
if gate_failure is not None:
    (OUT/'long-run-status.json').write_text(json.dumps(dict(status='EnvironmentBlocked',
        standard='NotExecuted', platformer='NotExecuted', evidence=str(gate_failure)), indent=2), encoding='utf-8')
    raise SystemExit(status)

standard = OUT/'standard-long-run-package'
from test_level_cycles import prepare
source=OUT/'standard-long-run-source'
prepare(source)
build_game(source/'project.azureproject',
    OUT/'arena-delivery/Moved Engine', standard)
rows=[]
for name, tool, package in [
    ('standard', 'run_playable_long_run.py', standard),
    ('platformer', 'run_platformer_long_run.py', OUT/'arena-delivery/Moved exploration')]:
    with (OUT/(name+'-long-run.log')).open('w', encoding='utf-8') as log:
        result=subprocess.run([sys.executable, '-X', 'utf8', str(ROOT/'tools'/tool),
            '--package', str(package), '--output', str(OUT/(name+'-long-run')),
            '--seconds', '1800'], stdout=log, stderr=subprocess.STDOUT)
    rows.append(dict(workload=name, exitCode=result.returncode,
        evidence=str(OUT/(name+'-long-run'))))
    (OUT/'long-run-status.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
    print(name, 'long run exit:', result.returncode, flush=True)
