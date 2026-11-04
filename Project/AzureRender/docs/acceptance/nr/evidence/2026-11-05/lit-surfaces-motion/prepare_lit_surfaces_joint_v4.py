from pathlib import Path
import os,json,sys,shutil,copy
r=Path(__file__).resolve().parent;root=r.parents[2];sys.path.insert(0,str(root/'tools'));from convert_material_semantics import read_asset,write_asset
p=r/'qualified-lit-surfaces-joint-v4';shutil.copytree(r/'qualified-lit-surfaces-joint-v3',p,copy_function=os.link,ignore=shutil.ignore_patterns('.azure','captures','output'));f=p/'content-character/hero-idle-opaque-overlays.glb';m,t=read_asset(f);m['materials'][18]['pbrMetallicRoughness']['baseColorTexture']=copy.deepcopy(m['materials'][19]['pbrMetallicRoughness']['baseColorTexture']);q=f.with_suffix('.update.glb');write_asset(q,m,t);os.replace(q,f)
registry={}
for f in p.rglob('*.azmeta'):
 d=json.loads(f.read_bytes());registry[d['id']]=(f,d)
level=json.loads((p/'scene/joint-dynamic-loop-v2.azurelevel').read_bytes());hero=next(n for n in level['nodes'] if n.get('id')=='hero');selected=set();fixed=set()
def image_id(d,i):return d['textures'][str(d['surface']['textures'][i]['source'])]['asset']
for name in ['name:M_actor_laevat_hair_01','name:captured-hair-front-blend']:
 path,_=registry[hero['materialSlots'][name]];d=json.loads(Path(str(path)[:-7]).read_bytes());mat=d['surface']['materials'][0];pr=mat['extras']['azureRenderMaterial'];h=pr['hair'];hr=h['projectedRamp']
 fixed.add(image_id(d,pr['diffuseRamp']['texture']));fixed.add(image_id(d,hr['rampTexture']));selected.add(image_id(d,mat['pbrMetallicRoughness']['baseColorTexture']['index']));selected.add(image_id(d,h['dataTexture']));selected.add(image_id(d,hr['parameterTexture']))
 if 'normalTexture' in mat:selected.add(image_id(d,mat['normalTexture']['index']))
records=[]
for identity in sorted(selected-fixed):
 f,d=registry[identity];old=copy.deepcopy(d.get('settings',{}));d.setdefault('settings',{}).setdefault('texture',{}).update(filter='linear',mipFilter='nearest',anisotropy=1,mipBias=-1);q=f.with_suffix('.update');q.write_text(json.dumps(d,indent=2));os.replace(q,f);records.append(dict(id=identity,path=str(f.relative_to(p)),before=old,after=d['settings']))
(p/'hair-source-samplers.json').write_text(json.dumps(dict(status='configured',materialSlots=len(hero['materialSlots']),records=records),indent=2));print('joint source samplers',len(records))
