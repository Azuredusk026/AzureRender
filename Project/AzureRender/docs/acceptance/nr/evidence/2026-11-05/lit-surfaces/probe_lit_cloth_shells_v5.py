from pathlib import Path
import os,json,shutil
r=Path(__file__).resolve().parent;p=r/'public-lit-cloth-shell-static-v5';shutil.copytree(r/'public-lit-cloth-shell-static-v4',p,copy_function=os.link,ignore=shutil.ignore_patterns('.azure','captures','output'))
for q in json.loads((p/'lit-cloth-shells.json').read_bytes())['records']:
 f=p/'materials'/(q['materialId']+'.azurematerial');d=json.loads(f.read_bytes());m=d['surface']['materials'][0];m.pop('normalTexture',None);m.pop('emissiveTexture',None);m['emissiveFactor']=[0,0,0];m['pbrMetallicRoughness'].pop('metallicRoughnessTexture',None);m['extras']['azureRenderMaterial']['nprDiffuse']['baseAo']='one'
 t=f.with_suffix('.update');t.write_text(json.dumps(d,indent=2),encoding='utf-8');os.replace(t,f)
s=(r/'capture_public_screen_shadow_head.py').read_text().replace('public-back-view-static','public-lit-cloth-shell-static-v5').replace('public-screen-shadow-head-capture','public-lit-cloth-shell-capture-v5');exec(compile(s,'cloth-contour-inputs','exec'))
