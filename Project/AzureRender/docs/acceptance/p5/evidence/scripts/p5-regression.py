"""Run the final frozen configurations; GPU workloads remain sequential."""
from pathlib import Path
import json
import subprocess
import sys
import time

root=Path.cwd();work=root/'build/implementation-2026-10-10'
group=sys.argv[1]
assert group in ('cpu','gpu')
rows=[]
for config in ('debug','release'):
    log=work/f'p5-final-{group}-{config}.log'
    start=time.monotonic()
    command=['ctest','--test-dir',str(root/f'build/ninja-msvc-{config}'),'-C',config.title(),
        '--output-on-failure','-L' if group=='gpu' else '-LE','gpu','-j','1']
    result=subprocess.run([sys.executable,str(work/'run-logged.py'),str(log),*command],stdout=subprocess.DEVNULL)
    text=log.read_text(encoding='utf-8')
    row=dict(config=config,group=group,exitCode=result.returncode,seconds=time.monotonic()-start,log=str(log),passed=result.returncode==0)
    rows.append(row)
    (work/f'p5-final-{group}-summary.json').write_text(json.dumps(dict(passed=all(r['passed'] for r in rows),checks=rows),indent=2)+'\n',encoding='utf-8')
    print(json.dumps(row),flush=True)
    if result.returncode:
        print(text[-10000:],flush=True)
        raise SystemExit(result.returncode)
