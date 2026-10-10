"""Recheck affected production paths serially against unchanged tested products."""
import json,subprocess,time
from pathlib import Path
ROOT=Path.cwd().resolve();WORK=ROOT/'build/implementation-2026-10-10'
records=[]
pattern=r'^(AzureEditor\.(ProjectFlow|ProductizationGpu|SettingsHost)|AzureEngine\.(SettingsOverlay|EditorPlayable))$'
for config in ('debug','release'):
    start=time.monotonic();log=WORK/f'p5-closeout-{config}.log'
    with log.open('w',encoding='utf-8') as stream:
        result=subprocess.run(['ctest','--test-dir',str(ROOT/f'build/ninja-msvc-{config}'),
            '-R',pattern,'-j','1','--output-on-failure'],stdout=stream,stderr=subprocess.STDOUT,cwd=ROOT)
    text=log.read_text(encoding='utf-8')
    passed=result.returncode==0 and '100% tests passed, 0 tests failed out of 5' in text
    records.append(dict(config=config,passed=passed,exitCode=result.returncode,tests=5,
        log=log.name,seconds=time.monotonic()-start))
    print(json.dumps(records[-1]),flush=True)
    if not passed:break
summary=dict(passed=len(records)==2 and all(r['passed'] for r in records),checks=records)
(WORK/'p5-closeout-tests.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8')
assert summary['passed'],'Affected production regressions failed'
