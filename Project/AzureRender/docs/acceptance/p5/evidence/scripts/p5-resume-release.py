"""Complete the four GPU cases left after the interrupted Release run."""
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

ROOT=Path.cwd().resolve();WORK=ROOT/'build/implementation-2026-10-10'
source=WORK/'p5-final-gpu-release.log'
text=source.read_text(encoding='utf-8')
assert len(re.findall(r'\d+/62 Test.*?Passed',text))==58
assert '***Failed' not in text and '***Timeout' not in text
shutil.copy2(source,WORK/'p5-gpu-release-interrupted.log')
data=json.loads((WORK/'p5-final-gpu-summary.json').read_text(encoding='utf-8'))
assert data['checks'][0]['passed'] and data['checks'][1]['exitCode']==1073807364
start=time.monotonic()
result=subprocess.run([sys.executable,WORK/'run-logged.py',WORK/'p5-gpu-release-remaining.log',
    'ctest','--test-dir',ROOT/'build/ninja-msvc-release','-C','Release','--output-on-failure',
    '-R','^(AzureGame.ExplorationCompletion|AzureEditor.AuthoringGpu|AzureEditor.SimulationGpu|AzureEditor.ViewportNavigationGpu)$','-j','1'],stdout=subprocess.DEVNULL)
data['checks'][1]=dict(config='release',group='gpu',passed=result.returncode==0,exitCode=result.returncode,
    tests=62,interruptedExitCode=1073807364,completedBeforeInterruption=58,remainingTests=4,
    coverage='58 full-suite passes plus four remaining cases after interruption',
    initialLog=str(WORK/'p5-gpu-release-interrupted.log'),remainingLog=str(WORK/'p5-gpu-release-remaining.log'),
    remainingSeconds=time.monotonic()-start)
data['passed']=all(r['passed'] for r in data['checks'])
(WORK/'p5-final-gpu-summary.json').write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8')
print(json.dumps(data['checks'][1]),flush=True)
raise SystemExit(result.returncode)
