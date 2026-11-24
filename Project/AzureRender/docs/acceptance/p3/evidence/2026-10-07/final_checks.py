import json, subprocess, os
from pathlib import Path
root=Path(__file__).resolve().parents[3];work=root/'build/evolution/p3'
os.environ['PYTHONUTF8']='1'
def run(name,args):
    with (work/(name+'.log')).open('w',encoding='utf-8') as log:
        r=subprocess.run(args,cwd=root,stdout=log,stderr=subprocess.STDOUT)
    if r.returncode:raise RuntimeError(name+': '+(work/(name+'.log')).read_text(encoding='utf-8',errors='replace')[-6000:])
    print(name+' passed',flush=True)
run('docs-check',['D:/Git/Git/bin/bash.exe','tools/check_docs.sh'])
docs=['../../README.md','README.md','docs/index.md','docs/plans/2026-10-06-engine-evolution.md',
      'docs/plans/azure-engine-plan.md','docs/plans/2026-10-06-engine-specialized-steps.md',
      'docs/plans/p3-implementation.md','docs/runtime/engine-delivery.md','docs/tutorials/engine-reuse.md',
      'docs/runtime/script-services.md','docs/runtime/playable-performance.md','docs/acceptance/p3/2026-10-07.md','CHANGELOG.md']
run('docs-links',['python','C:/Users/23587/.codex/skills/developer-documentation/scripts/validate_docs.py',*docs])
run('docs-style',['python','tools/check_doc_style.py',*docs])
run('docs-site',['python','-m','mkdocs','build','--strict','--site-dir',str(work/'site')])
for config in ('debug','release'):
    run('final-'+config+'-provenance',['python','tools/build_provenance.py','verify','--build-dir','build/ninja-msvc-'+config,'--config',config.title()])
run('final-contracts',['python','tools/write_engine_contracts.py','--verify','--output','build/ninja-msvc-release/release-gate/install-moved/share/AzureRender/contracts'])
(work/'final-checks.json').write_text(json.dumps({'status':'passed','checks':['documentation','links','style','strict-site','two-configurations-provenance','installed-contracts']},indent=2)+'\n')
