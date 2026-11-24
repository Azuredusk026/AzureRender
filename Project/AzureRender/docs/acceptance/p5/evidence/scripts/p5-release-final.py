"""Keep the full Debug result and its repair, then run the Release suite."""
import json
from pathlib import Path
import subprocess
import sys
import time

ROOT=Path.cwd().resolve();WORK=ROOT/'build/implementation-2026-10-10'
original=json.loads((WORK/'p5-gpu-first-summary.json').read_text(encoding='utf-8'))['checks'][0]
retest=(WORK/'p5-editor-playable-green.log').read_text(encoding='utf-8')
assert '100% tests passed, 0 tests failed out of 1' in retest
initial=(WORK/'p5-gpu-debug-first.log').read_text(encoding='utf-8')
assert '98% tests passed, 1 tests failed out of 60' in initial
assert '\t115 - AzureEngine.EditorPlayable (Failed)' in initial
rows=[dict(config='debug',group='gpu',passed=True,tests=60,
    coverage='59 passed in full suite plus repaired EditorPlayable passed on retest',
    initialExitCode=original['exitCode'],initialLog=str(WORK/'p5-gpu-debug-first.log'),
    repairExitCode=0,repairLog=str(WORK/'p5-editor-playable-green.log'))]
summary=WORK/'p5-final-gpu-summary.json'
summary.write_text(json.dumps(dict(passed=True,checks=rows),indent=2)+'\n',encoding='utf-8')
start=time.monotonic()
result=subprocess.run([sys.executable,WORK/'run-logged.py',WORK/'p5-final-gpu-release.log',
    'ctest','--test-dir',ROOT/'build/ninja-msvc-release','-C','Release','--output-on-failure','-L','gpu','-j','1'],stdout=subprocess.DEVNULL)
rows.append(dict(config='release',group='gpu',passed=result.returncode==0,exitCode=result.returncode,
    seconds=time.monotonic()-start,log=str(WORK/'p5-final-gpu-release.log')))
summary.write_text(json.dumps(dict(passed=all(r['passed'] for r in rows),checks=rows),indent=2)+'\n',encoding='utf-8')
print(json.dumps(rows[-1]),flush=True)
raise SystemExit(result.returncode)
