"""Run exclusive GPU acceptance after both final regressions terminate."""
import json
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path.cwd().resolve()
WORK = ROOT/'build/implementation-2026-10-10'
OUT = WORK/'p5-pipeline-summary.json'
rows = []

while True:
    ready = True
    for group in ('cpu', 'gpu'):
        path = WORK/f'p5-final-{group}-summary.json'
        if not path.exists():
            ready = False
            continue
        data = json.loads(path.read_text(encoding='utf-8'))
        if not data['passed']:
            raise RuntimeError(f'Final {group} regression failed; exclusive acceptance not started')
        ready &= len(data['checks']) == 2
    if ready:
        break
    time.sleep(5)

for name, args in [
    ('refresh-install', [sys.executable, WORK/'p5-refresh-install.py']),
    ('performance', [sys.executable, WORK/'p5-performance.py']),
    ('delivery', [sys.executable, WORK/'p5-delivery.py']),
    ('long-run', [sys.executable, ROOT/'tools/run_playable_long_run.py',
        '--package', ROOT/'build/ninja-msvc-release/playable-package/Azure Exploration',
        '--output', WORK/'p5-long-run', '--seconds', '1800']),
]:
    print('Starting exclusive', name, flush=True)
    start = time.monotonic()
    result = subprocess.run([sys.executable, WORK/'run-logged.py', WORK/f'p5-{name}.log', *args])
    rows.append(dict(stage=name, exitCode=result.returncode, seconds=time.monotonic()-start))
    OUT.write_text(json.dumps(dict(passed=all(r['exitCode']==0 for r in rows), checks=rows), indent=2)+'\n', encoding='utf-8')
    if result.returncode:
        raise SystemExit(result.returncode)
print('All exclusive acceptance complete', flush=True)
