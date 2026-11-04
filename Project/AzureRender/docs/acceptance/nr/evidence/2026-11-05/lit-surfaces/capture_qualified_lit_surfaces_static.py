from pathlib import Path
import os,json,sys,shutil,copy,subprocess
r=Path(__file__).resolve().parent;root=r.parents[2];sys.path.insert(0,str(root/'tools'));from convert_material_semantics import read_asset,write_asset
p=r/'qualified-lit-surfaces-static';shutil.copytree(r/'public-lit-cloth-shell-static-v8',p,copy_function=os.link,ignore=shutil.ignore_patterns('.azure','captures','output'))
def write(f,d):
 q=f.with_suffix('.update');q.write_text(json.dumps(d,indent=2),encoding='utf-8');os.replace(q,f)
bg=r/'public-matched-background-static-v5'
for name in ['native-empty-background.gltf','native-empty-background.png','native-empty-background.png.azmeta']:os.link(bg/'character'/name,p/'character'/name)
f=p/'scene/static-qualified.azurelevel';level=json.loads(f.read_bytes());reference=json.loads((bg/'scene/static-qualified.azurelevel').read_bytes());level['resources'].append(next(v for v in reference['resources'] if v['id']=='replay-background'));level['nodes'].append(next(v for v in reference['nodes'] if v['id']=='replay-background'));write(f,level)
f=p/'character/captured-opaque-overlays.glb';m,t=read_asset(f);m['materials'][18]['pbrMetallicRoughness']['baseColorTexture']=copy.deepcopy(m['materials'][19]['pbrMetallicRoughness']['baseColorTexture']);q=f.with_suffix('.update.glb');write_asset(q,m,t);os.replace(q,f)
hero=next(n for n in level['nodes'] if n.get('id')=='hero');selected=set();fixed=set()
def id_for(d,t):return d['textures'][str(d['surface']['textures'][t]['source'])]['asset']
for name,identity in hero['materialSlots'].items():
 d=json.loads((p/'materials'/(identity+'.azurematerial')).read_bytes());mat=d['surface']['materials'][0];pr=mat['extras'].get('azureRenderMaterial',{})
 for field in ['diffuseRamp','specularRamp','shadowColorLut']:
  if field in pr:fixed.add(id_for(d,pr[field]['texture']))
 if name not in ['name:M_actor_laevat_hair_01','name:captured-hair-front-blend']:continue
 for field in ['normalTexture']:
  if field in mat:selected.add(id_for(d,mat[field]['index']))
 if 'baseColorTexture' in mat.get('pbrMetallicRoughness',{}):selected.add(id_for(d,mat['pbrMetallicRoughness']['baseColorTexture']['index']))
 h=pr.get('hair',{})
 if 'dataTexture' in h:selected.add(id_for(d,h['dataTexture']))
 if 'parameterTexture' in h.get('projectedRamp',{}):selected.add(id_for(d,h['projectedRamp']['parameterTexture']))
selected-=fixed;found=set()
for f in p.rglob('*.azmeta'):
 d=json.loads(f.read_bytes())
 if d.get('id') in selected:
  found.add(d['id']);d.setdefault('settings',{}).setdefault('texture',{}).update(filter='linear',mipFilter='nearest',anisotropy=1,mipBias=-1);write(f,d)
assert found==selected and len(hero['materialSlots'])==14
write(p/'qualified-source-inputs.json',dict(status='configured',materialSlots=14,hairImplicitSamplerImages=sorted(selected),backgroundSource='native-empty-stage-v2',backgroundOnly=True,bodySamplingPreserved=True))
for cfg in ['release','debug']:
 s=(r/'capture_public_screen_shadow_head.py').read_text().replace('public-back-view-static','qualified-lit-surfaces-static').replace('public-screen-shadow-head-capture','qualified-lit-surfaces-static-'+cfg).replace('ninja-msvc-release','ninja-msvc-'+cfg);exec(compile(s,'qualified-lit-surfaces-'+cfg,'exec'));print(cfg,'captured',flush=True)
