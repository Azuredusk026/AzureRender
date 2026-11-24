"""Verify the final documentation and repository before archiving and committing."""
import json,os,subprocess,time
from pathlib import Path
ROOT=Path.cwd().resolve();WORK=ROOT/'build/implementation-2026-10-10'
docs=['README.md','CHANGELOG.md','docs/index.md','docs/plans/azure-engine-plan.md',
      'docs/plans/2026-10-09-editor-productization.md','docs/acceptance/p5/2026-10-10.md',
      'docs/acceptance/p5/physical-input.md','docs/runtime/project-workspace.md','portfolio/README_CN.md']
commands=[('style',['python','tools/check_doc_style.py',*docs]),
          ('audit',['python','tools/audit_repository.py','--source','../..','--output',str(WORK/'p5-closeout-repository-audit.json')]),
          ('site',['python','-m','mkdocs','build','--strict','--site-dir',str(WORK/'p5-site-closeout-final')])]
records=[]
for name,command in commands:
    start=time.monotonic();log=WORK/f'p5-closeout-{name}-final.log'
    with log.open('w',encoding='utf-8') as stream:
        result=subprocess.run(command,cwd=ROOT,stdout=stream,stderr=subprocess.STDOUT,
            env=dict(os.environ,PYTHONIOENCODING='utf-8',PYTHONUTF8='1'))
    records.append(dict(name=name,exitCode=result.returncode,passed=result.returncode==0,seconds=time.monotonic()-start,log=log.name))
    print(json.dumps(records[-1]),flush=True)
    assert result.returncode==0,(name,log)
summary=dict(passed=all(r['passed'] for r in records),checks=records)
(WORK/'p5-closeout-static.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8')
