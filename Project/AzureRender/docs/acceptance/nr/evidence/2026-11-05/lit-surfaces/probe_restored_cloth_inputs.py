from pathlib import Path
import os,json,shutil,copy
r=Path(__file__).resolve().parent
for base,dest,capture,lit in [('public-source-cube-static','public-restored-cloth-static','public-restored-cloth-capture',False),('public-lit-cloth-shell-static-v5','public-lit-cloth-shell-static-v6','public-lit-cloth-shell-capture-v6',True)]:
 p=r/dest;shutil.copytree(r/base,p,copy_function=os.link,ignore=shutil.ignore_patterns('.azure','captures','output'))
 def write(f,d):
  q=f.with_suffix('.update');q.write_text(json.dumps(d,indent=2),encoding='utf-8');os.replace(q,f)
 if lit:
  for q in json.loads((p/'lit-cloth-shells.json').read_bytes())['records']:
   f=p/'materials'/(q['materialId']+'.azurematerial');d=json.loads(f.read_bytes());pr=d['surface']['materials'][0]['extras']['azureRenderMaterial'];pr['specularInput'].update(source='emissive-alpha',scale=0);write(f,d)
 else:
  d=json.loads((p/'scene/static-qualified.azurelevel').read_bytes());h=next(n for n in d['nodes'] if n.get('id')=='hero')
  for n in ['name:M_actor_laevat_cloth_01','name:M_actor_laevat_cloth_02']:
   f=p/'materials'/(h['materialSlots'][n]+'.azurematerial');d=json.loads(f.read_bytes());d['surface']['materials'][0]['extras']['azureRenderMaterial']['outlineSurface'].update(color=[.5,.5,.5],baseColorSaturation=1.5);write(f,d)
 s=(r/'capture_public_screen_shadow_head.py').read_text().replace('public-back-view-static',dest).replace('public-screen-shadow-head-capture',capture);exec(compile(s,capture,'exec'));print(capture,'captured',flush=True)
