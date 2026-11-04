from pathlib import Path
import os,json,shutil,copy
r=Path(__file__).resolve().parent;p=r/'public-lit-cloth-shell-static-v7';shutil.copytree(r/'public-lit-cloth-shell-static-v6',p,copy_function=os.link,ignore=shutil.ignore_patterns('.azure','captures','output'))
def write(f,d):
 q=f.with_suffix('.update');q.write_text(json.dumps(d,indent=2),encoding='utf-8');os.replace(q,f)
ref=r/'public-restored-cloth-static';f=p/'scene/static-qualified.azurelevel';level=json.loads(f.read_bytes());level['reflectionProbe']=json.loads((ref/'scene/static-qualified.azurelevel').read_bytes())['reflectionProbe'];os.link(ref/'scene/cloth-captured-environment.azenv',p/'scene/cloth-captured-environment.azenv');write(f,level);h=next(n for n in level['nodes'] if n.get('id')=='hero')
for name in ['name:M_actor_laevat_cloth_01','name:M_actor_laevat_cloth_02']:
 f=p/'materials'/(h['materialSlots'][name]+'.azurematerial');d=json.loads(f.read_bytes());d['surface']['materials'][0]['extras']['azureRenderMaterial']['environmentLighting'].update(reflection='scene',specularModel='ggx-multiscatter-fit');write(f,d)
for q in json.loads((p/'lit-cloth-shells.json').read_bytes())['records']:
 f=p/'materials'/(q['materialId']+'.azurematerial');d=json.loads(f.read_bytes());d['surface']['materials'][0]['extras']['azureRenderMaterial']['nprDiffuse']['rimResponse']='view-facing';write(f,d)
s=(r/'capture_public_screen_shadow_head.py').read_text().replace('public-back-view-static','public-lit-cloth-shell-static-v7').replace('public-screen-shadow-head-capture','public-lit-cloth-shell-capture-v7');exec(compile(s,'source-facing-cloth-contours','exec'))
