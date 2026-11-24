from pathlib import Path
import os,json,shutil,math,struct,uuid,copy,numpy as np
from PIL import Image
r=Path(__file__).resolve().parent;p=r/'qualified-background-geometry-static';shutil.copytree(r/'qualified-lit-surfaces-static',p,copy_function=os.link,ignore=shutil.ignore_patterns('.azure','captures','output'))
manifest=json.loads((r/'qualified-lit-surfaces-static-release/frames/capture_manifest.json').read_bytes());origin=np.array(manifest['qaCameraPosition']);forward=np.array(manifest['qaCameraTarget'])-origin;forward/=np.linalg.norm(forward);up=np.array(manifest['cameraUp']);right=np.cross(forward,up);right/=np.linalg.norm(right);up=np.cross(right,forward);lens=manifest['cameraLens'];tanH=math.tan(lens['verticalFovRadians']/2);tanW=tanH*2560/1440;offset=lens['offsetNdc']
v=np.fromfile(r/'native-stage-460/vertices.bin',dtype='<f4').reshape(-1,26).astype('float64');idx=np.fromfile(r/'native-stage-460/indices.bin',dtype='<u2').reshape(-1,3)
planes=[np.array([1,0,0,1]),np.array([-1,0,0,1]),np.array([0,1,0,1]),np.array([0,-1,0,1])]
def clipped(poly):
 for plane in planes:
  result=[]
  for a,b in zip(poly,poly[1:]+poly[:1]):
   da=np.dot(a[:4],plane);db=np.dot(b[:4],plane)
   if da>=0:result.append(a)
   if (da>=0)!=(db>=0):result.append(a+(b-a)*(da/(da-db)))
  poly=result
  if not poly:return []
 return poly
triangles=[]
for ids in idx:
 poly=clipped([np.r_[v[i,:4],v[i,8:15]] for i in ids])
 if not poly or min(t[3] for t in poly)<=.1:continue
 for k in range(1,len(poly)-1):triangles.append(np.array([poly[0],poly[k],poly[k+1]]))
parents=len(triangles)
for depth in range(3):
 children=[]
 for tri in triangles:
  a,b,c=tri;ab=(a+b)/2;bc=(b+c)/2;ca=(c+a)/2
  children.extend([np.array([a,ab,ca]),np.array([ab,b,bc]),np.array([ca,bc,c]),np.array([ab,bc,ca])])
 triangles=children
positions=[];normals=[];uv=[];tangents=[]
for tri in triangles:
 data=[]
 for item in tri:
  x,y,z,w=item[:4];pos=origin+forward*w+right*((x-offset[0]*w)*tanW)+up*((-y+offset[1]*w)*tanH);normal=item[4:7]*np.array([1,-1,-1]);normal/=np.linalg.norm(normal);tangent=item[7:10]*np.array([-1,1,1]);tangent/=np.linalg.norm(tangent);data.append((pos,normal,np.array([.5+.5*x/w,.5+.5*y/w]),np.r_[tangent,item[10]]))
 if np.dot(np.cross(data[1][0]-data[0][0],data[2][0]-data[0][0]),origin-data[0][0])<0:data=[data[0],data[2],data[1]]
 for pos,n,tcoord,t in data:positions.append(pos);normals.append(n);uv.append(tcoord);tangents.append(t)
arrays=[np.asarray(positions,dtype='<f4'),np.asarray(normals,dtype='<f4'),np.asarray(uv,dtype='<f4'),np.asarray(tangents,dtype='<f4'),np.arange(len(positions),dtype='<u4')];blob=bytearray();views=[];accessors=[]
for i,a in enumerate(arrays):
 views.append(dict(buffer=0,byteOffset=len(blob),byteLength=a.nbytes));blob.extend(a.tobytes());accessors.append(dict(bufferView=i,componentType=5125 if i==4 else 5126,count=len(a),type=['VEC3','VEC3','VEC2','VEC4','SCALAR'][i]))
accessors[0].update(min=arrays[0].min(0).tolist(),max=arrays[0].max(0).tolist())
mat=copy.deepcopy(json.loads((p/'character/native-empty-background.gltf').read_bytes())['materials'][0]);mat.update(name='captured-stage-sphere',alphaMode='MASK',alphaCutoff=.01,doubleSided=False,normalTexture=dict(index=1));mat['pbrMetallicRoughness']['baseColorTexture']['index']=0
model=dict(asset=dict(version='2.0',generator='Native stage 460 post-VS geometry and fixed-view colour qualification'),scene=0,scenes=[dict(nodes=[0])],nodes=[dict(mesh=0)],buffers=[dict(uri='native-stage-sphere.bin',byteLength=len(blob))],bufferViews=views,accessors=accessors,meshes=[dict(primitives=[dict(attributes=dict(POSITION=0,NORMAL=1,TEXCOORD_0=2,TANGENT=3),indices=4,material=0)])],materials=[mat],samplers=[dict(minFilter=9728,magFilter=9728,wrapS=33071,wrapT=33071)],images=[dict(uri='native-empty-background.png'),dict(uri='native-stage-flat-normal.png')],textures=[dict(source=0,sampler=0),dict(source=1,sampler=0)],extensionsUsed=['KHR_materials_unlit'])
folder=p/'character';(folder/'native-stage-sphere.bin').write_bytes(blob);(folder/'native-stage-sphere.gltf').write_text(json.dumps(model,indent=2),encoding='utf-8');Image.fromarray(np.fromfile(r/'native-stage-460/ResourceId-336.bin',dtype='uint8').reshape(4,4,4)).save(folder/'native-stage-flat-normal.png');(folder/'native-stage-flat-normal.png.azmeta').write_text(json.dumps(dict(schemaVersion=1,id=str(uuid.uuid5(uuid.NAMESPACE_URL,'azure/native-stage-460-flat-normal')),dependencies=[],settings=dict(texture=dict(usage='normal',colorSpace='linear',filter='nearest',mipPolicy='none',anisotropy=1)))),encoding='utf-8')
f=p/'scene/static-qualified.azurelevel';level=json.loads(f.read_bytes());next(x for x in level['resources'] if x['id']=='replay-background')['asset']='character:/native-stage-sphere.gltf';q=f.with_suffix('.update');q.write_text(json.dumps(level,indent=2),encoding='utf-8');os.replace(q,f)
(p/'background-geometry-inputs.json').write_text(json.dumps(dict(sourceEvent=460,sourceIndices=2304,sourceVertices=509,clippedTriangles=parents,renderedTriangles=len(triangles),renderedVertices=len(positions),subdivisionLevels=3,sourceNormalTexture='ResourceId::336',sourceGeometryRetained=True,fixedViewColourOnly=True),indent=2));print('geometry prepared',len(triangles),flush=True)
s=(r/'capture_public_screen_shadow_head.py').read_text().replace('public-back-view-static','qualified-background-geometry-static').replace('public-screen-shadow-head-capture','qualified-background-geometry-capture');exec(compile(s,'background-geometry','exec'))
