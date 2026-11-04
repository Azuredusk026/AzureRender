from pathlib import Path
import os,sys,json,shutil
r=Path(__file__).resolve().parent;root=r.parents[2];p=r/'public-lit-cloth-shell-static-v4';shutil.copytree(r/'public-lit-cloth-shell-static-v3',p,copy_function=os.link,ignore=shutil.ignore_patterns('.azure','captures','output'))
for q in json.loads((p/'lit-cloth-shells.json').read_bytes())['records']:
 f=p/'materials'/(q['materialId']+'.azurematerial');d=json.loads(f.read_bytes());d['surface']['materials'][0]['pbrMetallicRoughness']['metallicFactor']=0
 tmp=f.with_suffix('.update');tmp.write_text(json.dumps(d,indent=2),encoding='utf-8');os.replace(tmp,f)
s=(r/'capture_public_screen_shadow_head.py').read_text().replace('public-back-view-static','public-lit-cloth-shell-static-v4').replace('public-screen-shadow-head-capture','public-lit-cloth-shell-capture-v4');exec(compile(s,'nonmetal-cloth-shell','exec'))
