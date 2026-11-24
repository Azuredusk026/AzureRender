from pathlib import Path
import os,sys,json,shutil,copy,uuid,hashlib
r=Path(__file__).resolve().parent;root=r.parents[2];sys.path.insert(0,str(root/'tools'));from convert_material_semantics import read_asset,write_asset
p=r/'public-lit-cloth-shell-static-v8';shutil.copytree(r/'public-lit-cloth-shell-static-v7',p,copy_function=os.link,ignore=shutil.ignore_patterns('.azure','captures','output'))
def write(f,d):
 q=f.with_suffix('.update');q.write_text(json.dumps(d,indent=2),encoding='utf-8');os.replace(q,f)
registry={}
for f in p.rglob('*.azmeta'):
 d=json.loads(f.read_bytes());registry[d['id']]=(Path(str(f)[:-7]),d)
f=p/'character/captured-opaque-overlays.glb';model,tail=read_asset(f);l=p/'scene/static-qualified.azurelevel';level=json.loads(l.read_bytes());hero=next(n for n in level['nodes'] if n.get('id')=='hero');original_slots=copy.deepcopy(hero['materialSlots']);records=[];copies={};image_dir=p/'character/derived-textures';image_dir.mkdir()
for row in json.loads((p/'lit-cloth-shells.json').read_bytes())['records']:
 doc=json.loads((p/'materials'/(row['materialId']+'.azurematerial')).read_bytes());material=copy.deepcopy(doc['surface']['materials'][0]);offset=len(model['textures']);sampler_offset=len(model.setdefault('samplers',[]));model['samplers'].extend(copy.deepcopy(doc['surface'].get('samplers',[])))
 for tex in doc['surface']['textures']:
  tex=copy.deepcopy(tex);binding=doc['textures'][str(tex['source'])];identity=binding['asset']
  if identity not in copies:
   src,meta=registry[identity];dest_id=str(uuid.uuid5(uuid.NAMESPACE_URL,'azure-render/embedded-derived-texture/'+identity));dest=image_dir/(dest_id+src.suffix);os.link(src,dest);metadata=copy.deepcopy(meta);metadata['id']=dest_id;write(dest.with_suffix(dest.suffix+'.azmeta'),metadata);image_index=len(model['images']);model['images'].append(dict(uri='derived-textures/'+dest.name));copies[identity]=dict(image=image_index,path=str(dest.relative_to(p)),sha256=hashlib.sha256(src.read_bytes()).hexdigest(),meta=metadata)
  tex['source']=copies[identity]['image']
  if 'sampler' in tex:tex['sampler']+=sampler_offset
  model['textures'].append(tex)
 def remap(value):
  if isinstance(value,dict):
   for k,v in value.items():
    if (k.endswith('Texture') or k=='texture') and type(v) in (int,float):value[k]=int(v)+offset
    elif k.endswith('Texture') and isinstance(v,dict) and 'index' in v:v['index']+=offset
    else:remap(v)
  elif isinstance(value,list):
   for v in value:remap(v)
 remap(material);model['materials'][row['derivedMaterial']]=material;assert hero['materialSlots'].pop('name:'+material['name'])==row['materialId'];records.append(dict(material=row['derivedMaterial'],name=material['name'],textureOffset=offset))
assert len(hero['materialSlots'])==14 and all(hero['materialSlots'][k]==v for k,v in original_slots.items() if k in hero['materialSlots']);q=f.with_suffix('.update.glb');write_asset(q,model,tail);os.replace(q,f);write(l,level);write(p/'embedded-derived-materials.json',dict(status='embedded',sourceMaterialSlots=14,records=records,textures=copies))
s=(r/'capture_public_screen_shadow_head.py').read_text().replace('public-back-view-static','public-lit-cloth-shell-static-v8').replace('public-screen-shadow-head-capture','public-lit-cloth-shell-capture-v8');exec(compile(s,'embedded-cloth-contours','exec'))
