from pathlib import Path
import os,sys,copy,json,shutil,uuid,numpy as np,base64,io,struct
from PIL import Image
r=Path(__file__).resolve().parent;root=r.parents[2];sys.path.insert(0,str(root/'tools'))
from convert_material_semantics import read_asset,write_asset,complete_material_document
p=r/'public-lit-cloth-shell-static-v3';shutil.copytree(r/'public-back-view-static',p,copy_function=os.link,ignore=shutil.ignore_patterns('.azure','captures','output'))
def write(path,data):
 q=path.with_suffix(path.suffix+'.update');q.write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8');os.replace(q,path)
f=p/'character/captured-opaque-overlays.glb';model,chunked_tail=read_asset(f);chunk_length,chunk_type=struct.unpack_from('<I4s',chunked_tail);assert chunk_type==b'BIN\0';tail=chunked_tail[8:8+chunk_length];suffix=chunked_tail[8+chunk_length:];payload=bytearray(tail);levelpath=p/'scene/static-qualified.azurelevel';level=json.loads(levelpath.read_bytes());hero=next(n for n in level['nodes'] if n.get('id')=='hero');original=copy.deepcopy(model['meshes']);records=[]
for index in [1,2]:
 src=model['materials'][index];identity=hero['materialSlots']['name:'+src['name']];path=p/'materials'/(identity+'.azurematerial');doc=json.loads(path.read_bytes());derived=copy.deepcopy(doc);mat=derived['surface']['materials'][0];profile=mat['extras']['azureRenderMaterial'];outline=copy.deepcopy(profile['outlineSurface']);outline['target']='surface';profile['outlineSurface']=outline
 profile['features']=[v for v in profile['features'] if v!='silhouette-outline'];profile.pop('specularRamp',None);profile['specularInput']['scale']=0
 npr=profile['nprDiffuse'];npr.update(rampComposition='view-facing-mask',lightingNormal='screen-surface',rimResponse='screen-depth')
 # Keep the source light colors, albedo/visibility policy and shadow lookup.
 record=model['images'][model['textures'][src['pbrMetallicRoughness']['baseColorTexture']['index']]['source']]
 if 'uri' in record:
  img=record['uri'];decoded=Image.open(io.BytesIO(base64.b64decode(img.split(',',1)[1]))) if img.startswith('data:') else Image.open(p/'character'/img)
 else:
  view=model['bufferViews'][record['bufferView']];offset=view.get('byteOffset',0);decoded=Image.open(io.BytesIO(tail[offset:offset+view['byteLength']]))
 rgba=np.asarray(decoded.convert('RGBA')).copy();rgb=rgba[:,:,:3].astype(float)/255;lin=np.where(rgb<=.04045,rgb/12.92,((rgb+.055)/1.055)**2.4)*.5;lum=lin@np.array([.21267290413379669,.7151522040367126,.07217500358819962]);graded=np.clip(lum[:,:,None]+(lin-lum[:,:,None])*1.5,0,1);rgba[:,:,:3]=np.rint(np.clip(np.where(graded<=.0031308,graded*12.92,1.055*graded**(1/2.4)-.055),0,1)*255).astype('uint8')
 image_id=str(uuid.uuid5(uuid.NAMESPACE_URL,'azure-render/captured-lit-cloth-outline-color-'+str(index)));image=p/'character'/('lit-cloth-'+str(index)+'-outline.png');Image.fromarray(rgba).save(image)
 write(image.with_suffix('.png.azmeta'),dict(schemaVersion=1,id=image_id,dependencies=[],settings=dict(texture=dict(usage='albedo',colorSpace='srgb',filter='linear',mipFilter='nearest',anisotropy=1,mipBias=-1))))
 source_ids={k:v['asset'] for k,v in derived['textures'].items()};new_image=max(map(int,source_ids))+1;source_ids[str(new_image)]=image_id;ti=len(derived['surface']['textures']);derived['surface']['textures'].append(dict(source=new_image));mat['pbrMetallicRoughness']['baseColorTexture']=dict(index=ti);mat['pbrMetallicRoughness']['baseColorFactor']=[1,1,1,1];mat['alphaMode']='OPAQUE';mat['doubleSided']=False
 name='captured-lit-cloth-'+str(index)+'-outline';mat['name']=name;derived=complete_material_document(mat,derived['surface'],source_ids);material_id=str(uuid.uuid5(uuid.NAMESPACE_URL,'azure-render/'+name));write(p/'materials'/(material_id+'.azurematerial'),derived);write(p/'materials'/(material_id+'.azurematerial.azmeta'),dict(schemaVersion=1,id=material_id,dependencies=sorted({v['asset'] for v in derived['textures'].values()}),settings={}));hero['materialSlots']['name:'+name]=material_id
 body_profile=doc['surface']['materials'][0]['extras']['azureRenderMaterial'];body_profile.pop('outlineSurface',None);body_profile['features']=[v for v in body_profile['features'] if v!='silhouette-outline'];write(path,doc)
 src['extras']['azureRenderMaterial'].pop('outlineSurface',None);src['extras']['azureRenderMaterial']['features']=[v for v in src['extras']['azureRenderMaterial']['features'] if v!='silhouette-outline']
 stub=copy.deepcopy(src);stub['name']=name;stub['extras']={};stub['doubleSided']=False;new_material=len(model['materials']);model['materials'].append(stub);count=0
 for mesh_i,mesh in enumerate(original):
  for prim in mesh['primitives']:
   if prim.get('material')!=index:continue
   assert prim.get('mode',4)==4
   acc=model['accessors'][prim['indices']];view=model['bufferViews'][acc['bufferView']];dtype={5123:'<u2',5125:'<u4'}[acc['componentType']];size=np.dtype(dtype).itemsize;assert view.get('byteStride',size)==size
   off=view.get('byteOffset',0)+acc.get('byteOffset',0);indices=np.frombuffer(tail,dtype=dtype,count=acc['count'],offset=off).reshape(-1,3)[:,[0,2,1]].copy();data=indices.tobytes();payload.extend(b'\0'*(-len(payload)%4));offset=len(payload);payload.extend(data);view_index=len(model['bufferViews']);model['bufferViews'].append(dict(buffer=0,byteOffset=offset,byteLength=len(data)));new_acc=len(model['accessors']);model['accessors'].append(dict(bufferView=view_index,componentType=acc['componentType'],count=acc['count'],type='SCALAR'));derived_primitive=copy.deepcopy(prim);derived_primitive.update(material=new_material,indices=new_acc);model['meshes'][mesh_i]['primitives'].append(derived_primitive);count+=1
 records.append(dict(sourceMaterial=index,derivedMaterial=new_material,primitives=count,materialId=material_id))
model['buffers'][0]['byteLength']=len(payload);payload.extend(b'\0'*(-len(payload)%4));new_tail=struct.pack('<I4s',len(payload),b'BIN\0')+bytes(payload)+suffix;q=f.with_suffix('.update.glb');write_asset(q,model,new_tail);os.replace(q,f);write(levelpath,level);write(p/'lit-cloth-shells.json',dict(status='generated',records=records,sourceDataPreserved=True,sourceMaterialSlots=14,derivedMaterialSlots=2))
s=(r/'capture_public_screen_shadow_head.py').read_text().replace('public-back-view-static','public-lit-cloth-shell-static-v3').replace('public-screen-shadow-head-capture','public-lit-cloth-shell-capture-v3')
exec(compile(s,'lit-cloth-shell-capture','exec'))
