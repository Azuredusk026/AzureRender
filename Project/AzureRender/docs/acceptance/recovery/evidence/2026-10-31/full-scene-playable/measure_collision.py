from pathlib import Path
import json,sys,numpy as np,hashlib
root=Path.cwd();r=Path(__file__).resolve().parent;sys.path.insert(0,str(root/'tools'));from retarget_animation import read_glb,trs_matrix;from endfield_native.mesh_uv import _read
project=json.loads((r/'project/playable.azureproject').read_text());mounts={m['name']:r/'project'/m['path'] for m in project['mounts']};triangles=[];records=[]
for name in ['structural-collision.glb','native-railing-collision.glb']:
 p=mounts['assets']/name;d,c=read_glb(p);b=next(x for kind,x in c if kind==0x004e4942);parents={child:i for i,n in enumerate(d['nodes']) for child in n.get('children',[])};worlds={}
 def world(i):
  if i not in worlds:
   n=d['nodes'][i];local=np.array(n['matrix']).reshape(4,4,order='F') if 'matrix' in n else trs_matrix((n.get('translation',[0,0,0]),n.get('rotation',[0,0,0,1]),n.get('scale',[1,1,1])));worlds[i]=(world(parents[i]) if i in parents else np.eye(4))@local
  return worlds[i]
 total=0
 for i,node in enumerate(d['nodes']):
  if 'mesh' not in node:continue
  for q in d['meshes'][node['mesh']]['primitives']:
   position=_read(d,b,q['attributes']['POSITION'],'VEC3',(5126,));indices=_read(d,b,q['indices'],'SCALAR',(5121,5123,5125)).astype(int).ravel();assert len(indices)%3==0;positions=(world(i)@np.c_[position,np.ones(len(position))].T).T[:,:3];triangles.extend(positions[indices].reshape(-1,3,3));total+=len(indices)//3
 records.append(dict(asset=name,triangles=total,sha256=hashlib.sha256(p.read_bytes()).hexdigest()))
triangles=np.asarray(triangles);a=triangles[:,0];e1=triangles[:,1]-a;e2=triangles[:,2]-a
rows=[]
for label,origin,direction,maximum in [('floor',[0,1.135,9],[0,-1,0],5),('forward-low',[0,1.4,9],[0,0,-1],12),('forward-mid',[0,1.9,9],[0,0,-1],12),('right',[0,1.4,9],[1,0,0],25)]:
 origin=np.array(origin);direction=np.array(direction);p=np.cross(np.broadcast_to(direction,e2.shape),e2);det=np.einsum('ij,ij->i',e1,p);valid=np.abs(det)>1e-10;inverse=np.zeros(len(det));inverse[valid]=1/det[valid];t=origin-a;u=np.einsum('ij,ij->i',t,p)*inverse;q=np.cross(t,e1);v=np.einsum('j,ij->i',direction,q)*inverse;distance=np.einsum('ij,ij->i',e2,q)*inverse;hit=valid&(u>=0)&(v>=0)&(u+v<=1)&(distance>=0)&(distance<=maximum);ids=np.flatnonzero(hit);ids=ids[np.argsort(distance[ids])];rows.append(dict(query=label,origin=origin.tolist(),direction=direction.tolist(),maximumDistance=maximum,hits=[dict(distance=float(distance[i]),point=(origin+direction*distance[i]).tolist(),triangle=int(i)) for i in ids[:5]]))
report=dict(status='collision-geometry-queries-measured',scope='CPU rays against declared rendered-geometry collision meshes. Capsule contact and native collision equivalence require actual physics checks.',sources=records,queries=rows);(r/'collision-geometry.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
