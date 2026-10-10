"""Include the reviewable acceptance evidence without changing tested products."""
import json, shutil, subprocess
from pathlib import Path
ROOT=Path.cwd();WORK=ROOT/'build/implementation-2026-10-10'
assert json.loads((WORK/'p5-long-run/summary.json').read_text(encoding='utf-8'))['actualSeconds']>=1800
p=ROOT/'CMakeLists.txt';text=p.read_text(encoding='utf-8');rule='\ninstall(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/docs/acceptance/p5/evidence/" DESTINATION "${CMAKE_INSTALL_DATADIR}/AzureRender/docs/acceptance/p5/evidence" OPTIONAL)\n'
assert rule.strip() not in text
with p.open('a',encoding='utf-8',newline='') as out:out.write(rule)
records={}
for config in ('debug','release'):
 p=ROOT/f'build/ninja-msvc-{config}/build-provenance.json';before=json.loads(p.read_text(encoding='utf-8'));shutil.copy2(p,WORK/f'p5-before-evidence-install-{config}.json')
 with (WORK/f'p5-evidence-install-build-{config}.log').open('w',encoding='utf-8') as out:
  result=subprocess.run(['cmd.exe','/d','/c',str(WORK/f'p5-build-{config}.cmd')],stdout=out,stderr=subprocess.STDOUT)
 assert result.returncode==0
 after=json.loads(p.read_text(encoding='utf-8'));assert before['products']==after['products']
 changed=[name for name,h in before['source'].items() if after['source'].get(name)!=h];assert changed==['CMakeLists.txt'],changed
 records[config]=dict(passed=True,productsUnchanged=True,changedInputs=changed)
 print(config,'acceptance evidence install registered, tested products unchanged',flush=True)
(WORK/'p5-evidence-install-verification.json').write_text(json.dumps(records,indent=2)+'\n',encoding='utf-8')
