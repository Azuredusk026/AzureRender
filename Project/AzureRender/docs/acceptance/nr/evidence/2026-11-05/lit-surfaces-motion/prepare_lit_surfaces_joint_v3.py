from pathlib import Path
import os,sys,json,shutil,copy,uuid,struct,hashlib,base64,numpy as np
r=Path(__file__).resolve().parent;root=r.parents[2];sys.path.insert(0,str(root/'tools'));from convert_material_semantics import read_asset,write_asset
source=r/'public-lit-cloth-shell-static-v8';p=r/'qualified-lit-surfaces-joint-v3';shutil.copytree(r/'skin-warm-joint',p,copy_function=os.link,ignore=shutil.ignore_patterns('.azure','captures','output'));source_file=source/'character/captured-opaque-overlays.glb';sm,st=read_asset(source_file);sn,sk=struct.unpack_from('<I4s',st);assert sk==b'BIN\0';source_bin=st[8:8+sn];f=p/'content-character/hero-idle-opaque-overlays.glb';m,tail=read_asset(f);old_model=copy.deepcopy(m);n,k=struct.unpack_from('<I4s',tail);assert k==b'BIN\0';raw=tail[8:8+n];suffix=tail[8+n:];payload=bytearray(raw);records=[];folder=p/'content-character/derived-textures';folder.mkdir();images={};textures={};metas={}
for meta in source.rglob('*.azmeta'):
 d=json.loads(meta.read_bytes());metas[d['id']]=(Path(str(meta)[:-7]),d)
def write(path,d):
 q=path.with_suffix('.update');q.write_text(json.dumps(d,indent=2),encoding='utf-8');os.replace(q,path)
def image(path,data=None,mime=None):
 if path is not None:
  b=path.read_bytes();side=path.with_suffix(path.suffix+'.azmeta');metadata=json.loads(side.read_bytes()) if side.exists() else dict(schemaVersion=1,dependencies=[],settings={});ext=path.suffix
 else:b=data;metadata=dict(schemaVersion=1,dependencies=[],settings={});ext='.png' if mime=='image/png' else '.jpg'
 digest=hashlib.sha256(b+json.dumps(metadata.get('settings',{}),sort_keys=True).encode()).hexdigest()
 if digest in images:return images[digest]
 identity=str(uuid.uuid5(uuid.NAMESPACE_URL,'azure-render/joint-derived-image/'+digest));dest=folder/(identity+ext);dest.write_bytes(b);metadata['id']=identity;write(dest.with_suffix(dest.suffix+'.azmeta'),metadata);i=len(m['images']);m['images'].append(dict(uri='derived-textures/'+dest.name));images[digest]=(i,identity);records.append(dict(kind='image',path=str(dest.relative_to(p)),sha256=hashlib.sha256(b).hexdigest(),id=identity));return images[digest]
def texture(index):
 if index in textures:return textures[index]
 t=copy.deepcopy(sm['textures'][index]);im=sm['images'][t['source']]
 if 'uri' in im:
  uri=im['uri'];ii,_=image(None,base64.b64decode(uri.split(',',1)[1]),uri.split(';',1)[0][5:]) if uri.startswith('data:') else image(source_file.parent/uri)
 else:
  v=sm['bufferViews'][im['bufferView']];off=v.get('byteOffset',0);ii,_=image(None,source_bin[off:off+v['byteLength']],im['mimeType'])
 t['source']=ii
 if 'sampler' in t:
  sampler=copy.deepcopy(sm['samplers'][t['sampler']]);t['sampler']=len(m.setdefault('samplers',[]));m['samplers'].append(sampler)
 ti=len(m['textures']);m['textures'].append(t);textures[index]=ti;return ti

def remap(value):
 if isinstance(value,dict):
  for key,child in value.items():
   if (key.endswith('Texture') or key=='texture') and type(child) in (int,float):value[key]=texture(int(child))
   elif key.endswith('Texture') and isinstance(child,dict) and 'index' in child:child['index']=texture(child['index'])
   else:remap(child)
 elif isinstance(value,list):
  for child in value:remap(child)
# Independent normal producer uses the source normal image and sampler.
sl=json.loads((source/'scene/static-qualified.azurelevel').read_bytes());sh=next(n for n in sl['nodes'] if n.get('id')=='hero');hd=json.loads((source/'materials'/(sh['materialSlots']['name:M_actor_laevat_hair_01']+'.azurematerial')).read_bytes());nt=hd['surface']['materials'][0]['normalTexture'];normal_texture=hd['surface']['textures'][nt['index']];normal_id=hd['textures'][str(normal_texture['source'])]['asset'];normal_image,normal_copy_id=image(metas[normal_id][0]);normal_index=len(m['textures']);m['textures'].append(dict(source=normal_image));m['materials'][9]['normalTexture']=dict(index=normal_index)
for index in [9]:
 fdoc=p/f'assets/linear-display-materials/material-{index}.azurematerial';d=json.loads(fdoc.read_bytes());mat=d['surface']['materials'][0];pr=mat['extras']['azureRenderMaterial']
 if index==9:
  pr['features']=[v for v in pr['features'] if v!='silhouette-outline'];pr['hair']['projectedRamp']['snapshotNormal']='material-normal-rg';image_slot=len(d['textures']);d['textures'][str(image_slot)]=dict(asset=normal_copy_id,usage='linear');ti=len(d['surface']['textures']);d['surface']['textures'].append(dict(source=image_slot));mat['normalTexture']=dict(index=ti);m['materials'][9]['extras']['azureRenderMaterial']['features']=[v for v in m['materials'][9]['extras']['azureRenderMaterial']['features'] if v!='silhouette-outline'];m['materials'][9]['extras']['azureRenderMaterial']['hair']['projectedRamp']['snapshotNormal']='material-normal-rg'
 write(fdoc,d)
# Add derived surfaces using the original animated primitives and reversed indices.
for source_index,base in [(18,9),(19,9),(20,1),(21,2)]:
 mat=copy.deepcopy(sm['materials'][source_index]);remap(mat)
 if source_index==18:mat['extras']['azureRenderMaterial']['nprDiffuse']['rimOrigin']['node']='Bip001_Head'
 mat['name']=['hair-back-projected-surface','hair-opaque-projected-surface','cloth-one-projected-surface','cloth-two-projected-surface'][source_index-18];new_index=len(m['materials']);m['materials'].append(mat);primitive_count=0
 for mesh_index,mesh in enumerate(old_model['meshes']):
  for prim in mesh['primitives']:
   if prim.get('material')!=base:continue
   a=m['accessors'][prim['indices']];v=m['bufferViews'][a['bufferView']];dtype={5123:'<u2',5125:'<u4'}[a['componentType']];off=v.get('byteOffset',0)+a.get('byteOffset',0);indices=np.frombuffer(raw,dtype=dtype,count=a['count'],offset=off).reshape(-1,3)[:,[0,2,1]].copy();data=indices.tobytes();payload.extend(b'\0'*(-len(payload)%4));off=len(payload);payload.extend(data);vi=len(m['bufferViews']);m['bufferViews'].append(dict(buffer=0,byteOffset=off,byteLength=len(data)));ai=len(m['accessors']);m['accessors'].append(dict(bufferView=vi,componentType=a['componentType'],count=a['count'],type='SCALAR'));new=copy.deepcopy(prim);new.update(material=new_index,indices=ai);m['meshes'][mesh_index]['primitives'].append(new);primitive_count+=1
 assert primitive_count>0;records.append(dict(kind='surface',source=base,material=new_index,primitives=primitive_count))
for index in [1,2]:
 for mat in [m['materials'][index]]:
  pr=mat['extras']['azureRenderMaterial'];pr.pop('outlineSurface',None);pr['features']=[v for v in pr['features'] if v!='silhouette-outline']
 fd=p/f'assets/linear-display-materials/material-{index}.azurematerial';d=json.loads(fd.read_bytes());pr=d['surface']['materials'][0]['extras']['azureRenderMaterial'];pr.pop('outlineSurface',None);pr['features']=[v for v in pr['features'] if v!='silhouette-outline'];write(fd,d)
assert m['animations']==old_model['animations'] and m['skins']==old_model['skins'] and m['nodes']==old_model['nodes'];assert bytes(payload[:len(raw)])==raw
for before,after in zip(old_model['meshes'],m['meshes']):assert after['primitives'][:len(before['primitives'])]==before['primitives']
m['buffers'][0]['byteLength']=len(payload);payload.extend(b'\0'*(-len(payload)%4));new_tail=struct.pack('<I4s',len(payload),b'BIN\0')+payload+suffix;q=f.with_suffix('.update.glb');write_asset(q,m,new_tail);os.replace(q,f)
for level_file in (p/'scene').glob('*.azurelevel'):
 d=json.loads(level_file.read_bytes())
 for node in d['nodes']:
  if node.get('id')=='hero':
   baseline=json.loads((r/'skin-warm-joint/scene'/level_file.name).read_bytes());expected=next(n for n in baseline['nodes'] if n.get('id')=='hero');assert node['materialSlots']==expected['materialSlots']
   if level_file.name=='joint-dynamic-loop-v2.azurelevel':assert len(node['materialSlots'])==14
for index in [0,8]:assert json.loads((p/f'assets/linear-display-materials/material-{index}.azurematerial').read_bytes())['surface']['materials'][0]['pbrMetallicRoughness']['baseColorFactor']==[.98,.93,.89,1]
write(p/'derived-lit-surfaces.json',dict(status='generated',sourceMaterialSlots=14,animations=len(m['animations']),geometryPayloadPrefixPreserved=True,records=records));print('joint derived materials ready',len(m['materials']),len(records),flush=True)
