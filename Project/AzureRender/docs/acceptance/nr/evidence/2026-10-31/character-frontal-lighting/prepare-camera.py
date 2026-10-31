from pathlib import Path
import json,sys,math,copy,os,hashlib
import numpy as np
root=Path.cwd();r=Path(__file__).resolve().parent;sys.path.insert(0,str(root/'tools'));from retarget_animation import read_glb,trs_matrix
source=root/'build/runs/20261031-105539-full-scene-character-composition-b37668/project';asset=source/'content-character/hero-restored-inputs-v7.glb';d,chunks=read_glb(asset);binary=next(data for kind,data in chunks if kind==0x004e4942)
def accessor(index):
 a=d['accessors'][index];v=d['bufferViews'][a['bufferView']];dtype={5126:'<f4',5125:'<u4',5123:'<u2',5121:'u1'}[a['componentType']];width={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4,'MAT4':16}[a['type']];data=np.ndarray((a['count'],width),dtype=dtype,buffer=binary,offset=v.get('byteOffset',0)+a.get('byteOffset',0),strides=(v.get('byteStride',np.dtype(dtype).itemsize*width),np.dtype(dtype).itemsize)).astype(float)
 if a.get('normalized') and a['componentType']!=5126:data/=np.iinfo(np.dtype(dtype)).max
 return data
transforms=[[n.get('translation',[0,0,0]),n.get('rotation',[0,0,0,1]),n.get('scale',[1,1,1])] for n in d['nodes']];time=1/60
for channel in d['animations'][0]['channels']:
 sampler=d['animations'][0]['samplers'][channel['sampler']];times=accessor(sampler['input'])[:,0];values=accessor(sampler['output']);left=max(0,min(len(times)-1,int(np.searchsorted(times,time,side='right'))-1));right=min(left+1,len(times)-1);alpha=0 if times[right]==times[left] or sampler.get('interpolation')=='STEP' else float(np.clip((time-times[left])/(times[right]-times[left]),0,1));a=values[left];b=values[right];path=channel['target']['path']
 if path=='rotation':
  dot=float(a@b)
  if dot<0:b=-b;dot=-dot
  if dot<.9995:angle=math.acos(min(1,dot));value=(math.sin((1-alpha)*angle)*a+math.sin(alpha*angle)*b)/math.sin(angle)
  else:value=(1-alpha)*a+alpha*b
  value/=np.linalg.norm(value)
 else:value=(1-alpha)*a+alpha*b
 if path in ('translation','rotation','scale'):transforms[channel['target']['node']][('translation','rotation','scale').index(path)]=value
parents=[-1]*len(d['nodes'])
for i,node in enumerate(d['nodes']):
 for child in node.get('children',[]):assert parents[child]<0;parents[child]=i
worlds={}
def world(i):
 if i not in worlds:
  node=d['nodes'][i];local=np.array(node['matrix']).reshape(4,4,order='F') if 'matrix' in node else trs_matrix(transforms[i]);worlds[i]=(world(parents[i]) if parents[i]>=0 else np.eye(4))@local
 return worlds[i]
scene=json.loads((source/'scene/full-character-visible.azurelevel').read_text());hero=next(n for n in scene['nodes'] if n['id']=='hero');t=hero['components']['azure.transform']['data'];yaw=math.radians(t['rotation'][1]);model=np.eye(4);model[:3,:3]=[[math.cos(yaw),0,math.sin(yaw)],[0,1,0],[-math.sin(yaw),0,math.cos(yaw)]];model[:3,3]=t['translation'];assert t['rotation'][0]==t['rotation'][2]==0 and t['scale']==[1,1,1]
meshIndex=next(i for i,m in enumerate(d['meshes']) if any(p.get('material')==11 for p in m['primitives']));nodeIndex=next(i for i,n in enumerate(d['nodes']) if n.get('mesh')==meshIndex);primitive=next(p for p in d['meshes'][meshIndex]['primitives'] if p.get('material')==11);a=primitive['attributes'];positions=accessor(a['POSITION']);normals=accessor(a['NORMAL']);joints=accessor(a['JOINTS_0']).astype(int);weights=accessor(a['WEIGHTS_0']);skin=d['skins'][d['nodes'][nodeIndex]['skin']];ib=accessor(skin['inverseBindMatrices']);palette=np.array([world(joint)@value.reshape(4,4,order='F') for joint,value in zip(skin['joints'],ib)]);matrices=np.sum(palette[joints]*weights[:,:,None,None],axis=1);posed=(matrices@np.concatenate([positions,np.ones((len(positions),1))],axis=1)[:,:,None])[:,:,0];posed=(model@posed.T).T[:,:3];posedNormals=(model[:3,:3]@np.einsum('nij,nj->ni',matrices[:,:3,:3],normals).T).T;posedNormals/=np.linalg.norm(posedNormals,axis=1)[:,None];center=posed.mean(0);normal=posedNormals.mean(0);normal/=np.linalg.norm(normal);up=np.array([0.,1.,0.]);right=np.cross(up,normal);right/=np.linalg.norm(right);up=np.cross(normal,right);rotation=np.stack([right,up,normal],axis=1);cy=math.hypot(rotation[1,2],rotation[2,2]);angles=np.degrees([math.atan2(rotation[1,2],rotation[2,2]),math.atan2(rotation[0,2],cy),math.atan2(rotation[0,1],rotation[0,0])]);cameraPosition=center+normal*.85
if '--measure-only' in sys.argv:
 def compose_angles(angles):
  x,y,z=np.radians(angles);return np.array([[1,0,0],[0,math.cos(x),math.sin(x)],[0,-math.sin(x),math.cos(x)]])@np.array([[math.cos(y),0,math.sin(y)],[0,1,0],[-math.sin(y),0,math.cos(y)]])@np.array([[math.cos(z),math.sin(z),0],[-math.sin(z),math.cos(z),0],[0,0,1]])
 measures=[]
 oldCamera=next(n for n in scene['nodes'] if n['id']=='camera')['components'];oldTransform=oldCamera['azure.transform']['data'];oldLens=oldCamera['azure.camera']['data']
 for label,position,angles,fov,width,height,offset in [('complete-scene',oldTransform['translation'],oldTransform['rotation'],oldLens['fieldOfView'],2560,1440,[oldLens['projectionOffsetX'],oldLens['projectionOffsetY']]),('frontal',cameraPosition,angles,32,1536,1536,[0,0])]:
  basis=compose_angles(angles);view=(basis.T@(posed-np.asarray(position)).T).T;depth=-view[:,2];assert np.all(depth>0);f=1/math.tan(math.radians(fov)/2);ndc=np.stack([view[:,0]*f/(width/height)/depth+offset[0],-view[:,1]*f/depth+offset[1]],axis=1);pixels=(ndc*.5+.5)*[width,height];low=pixels.min(0);high=pixels.max(0);groups=[]
  for left in (True,False):
   points=pixels[(pixels[:,0]<=pixels[:,0].mean())==left];a=points.min(0);b=points.max(0);groups.append(dict(group='image-left-half' if left else 'image-right-half',vertices=len(points),widthPixels=float(b[0]-a[0]),heightPixels=float(b[1]-a[1])))
  measures.append(dict(camera=label,resolution=[width,height],irisGeometryBoundsXYXY=[*low.tolist(),*high.tolist()],irisGeometryWidthPixels=float(high[0]-low[0]),irisGeometryHeightPixels=float(high[1]-low[1]),halfGroups=groups))
 report=dict(status='projected-geometry-measured',scope='CPU pose at 1/60 second. Projected geometry bounds include hidden surfaces and do not establish visible coverage or a pixel acceptance mask.',cases=measures);(r/'iris-pixel-footprint.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));raise SystemExit(0)
projectDir=r/'project';(projectDir/'scene').mkdir(parents=True);descriptor=json.loads((source/'full-character-visible.azureproject').read_text())
for mount in descriptor['mounts']:mount['path']='scene' if mount['name']=='stage' else os.path.relpath((source/mount['path']).resolve(),projectDir).replace('\\','/')
camera=next(n for n in scene['nodes'] if n['id']=='camera');camera['components']['azure.transform']['data'].update(translation=cameraPosition.tolist(),rotation=angles.tolist());camera['components']['azure.camera']['data'].update(fieldOfView=32,projectionOffsetX=0,projectionOffsetY=0);scene['renderSettings']['antiAliasing']=0;spec=json.loads((root/'build/runs/20261031-155126-temporal-reference-quality-a9c045/illumination-candidates.json').read_text());cases=[]
for name,preset in [('control',scene['renderSettings']['nprLighting']),('candidate',spec['appliedPreset'])]:
 candidate=copy.deepcopy(scene);candidate['renderSettings']['nprLighting']=preset;level=projectDir/'scene'/('frontal-'+name+'.azurelevel');level.write_text(json.dumps(candidate,indent=2)+'\n');config=copy.deepcopy(descriptor);config['id']='frontal-'+name;config['startupScene']='stage:/'+level.name;project=projectDir/(name+'.azureproject');project.write_text(json.dumps(config,indent=2)+'\n');cases.append(dict(name=name,project=str(project.relative_to(root)),levelSha256=hashlib.sha256(level.read_bytes()).hexdigest()))
report=dict(status='camera-candidate-prepared',baseCommit='06d701c',sourceAsset=str(asset.relative_to(root)),sourceAssetSha256=hashlib.sha256(asset.read_bytes()).hexdigest(),animation=d['animations'][0]['name'],cameraSamplingSeconds=time,irisVertices=len(posed),worldIrisCenter=center.tolist(),meanWorldIrisNormal=normal.tolist(),cameraPosition=cameraPosition.tolist(),cameraRotationDegrees=angles.tolist(),fieldOfView=32,cases=cases,scope='CPU pose only frames the diagnostic camera. Existing Player animation and rendering remain authoritative; no mesh, material, light, grade or shadow changes.');(r/'camera-candidate.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
