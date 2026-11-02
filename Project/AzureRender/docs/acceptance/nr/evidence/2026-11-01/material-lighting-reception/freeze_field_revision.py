import gzip,hashlib,json,shutil
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;dest=run/'before-field';dest.mkdir();sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();bindings=[]
for cfg in ['release','debug']:
 p=run/(cfg+'-build.json');d=json.loads(p.read_text());assert d['status']=='built-contract-tests-passed';assert sha(root/('build/ninja-msvc-'+cfg)/'AzurePlayer.exe')==d['programs']['AzurePlayer'];assert {p.name:sha(p) for p in (root/('build/ninja-msvc-'+cfg)/'shaders').glob('*.spv')}==d['shaders'];shutil.copy2(p,dest/p.name);shutil.copy2(run/('public-gpu-'+cfg+'.json'),dest/('public-gpu-'+cfg+'.json'));bindings.append(dict(configuration=cfg,buildRecordSha256=sha(p),programSha256=d['programs']['AzurePlayer'],shaders=d['shaders']))
for name in ['shaders/mesh.frag','tools/test_material_lighting_gpu.py']:
 p=root/name;q=dest/(name.replace('/','-')+'.gz');q.write_bytes(gzip.compress(p.read_bytes(),mtime=0))
for name in ['native-captures','native-capture-summary.json','native-capture-summary-fullbody-world-aa0,fullbody-source-aa0.json','native-body-aa0-driver.log','body-comparison-aa0.json','body-comparison-aa0.log','body-comparison-aa0.png']:
 p=run/name;q=dest/name;assert p.resolve().parent==run.resolve() and q.resolve().is_relative_to(run.resolve()) and not q.exists();p.rename(q)
(dest/'bindings.json').write_bytes((json.dumps(dict(status='frozen-before-probe-independence-fix',configurations=bindings,reason='Matched global diffuse selection must not change scene probe adaptation.',visualAcceptance='failed-user-review'),indent=2)+'\n').encode());print('Preserved both compiled packages, completed public controls and initial full-body captures')
