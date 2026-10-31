from pathlib import Path
import contextlib,hashlib,io,json,runpy
import numpy as np
r=Path(__file__).resolve().parent;sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();frozen=sha(r/'collision-geometry.json')
with contextlib.redirect_stdout(io.StringIO()):g=runpy.run_path(str(r/'measure_collision.py'))
assert sha(r/'collision-geometry.json')==frozen
a,e1,e2=[g[k] for k in ['a','e1','e2']];direction=np.array([0.,-1.,0.]);p=np.cross(np.broadcast_to(direction,e2.shape),e2);det=np.einsum('ij,ij->i',e1,p);valid=np.abs(det)>1e-10;inverse=np.zeros(len(det));inverse[valid]=1/det[valid]
d=json.loads((r/'moving-green/runtime.json').read_text());rows=[]
def sphere_contact(foot):
 center=foot+np.array([0,.3,0]);normal=np.cross(e1,e2);squared=np.einsum('ij,ij->i',normal,normal);safe=squared>1e-20
 scale=np.zeros(len(a));scale[safe]=np.einsum('ij,ij->i',a-center,normal)[safe]/squared[safe];projected=center+normal*scale[:,None]
 relative=projected-a;d00=np.einsum('ij,ij->i',e1,e1);d01=np.einsum('ij,ij->i',e1,e2);d11=np.einsum('ij,ij->i',e2,e2);d20=np.einsum('ij,ij->i',relative,e1);d21=np.einsum('ij,ij->i',relative,e2);den=d00*d11-d01*d01;ok=safe&(np.abs(den)>1e-20);u=np.zeros(len(a));v=np.zeros(len(a));u[ok]=(d11*d20-d01*d21)[ok]/den[ok];v[ok]=(d00*d21-d01*d20)[ok]/den[ok];inside=ok&(u>=0)&(v>=0)&(u+v<=1)
 candidates=[]
 for start,end in [(a,a+e1),(a+e1,a+e2),(a+e2,a)]:
  edge=end-start;length=np.einsum('ij,ij->i',edge,edge);t=np.zeros(len(a));mask=length>1e-20;t[mask]=np.clip(np.einsum('ij,ij->i',center-start,edge)[mask]/length[mask],0,1);candidates.append(start+edge*t[:,None])
 face=projected.copy();face[~inside]=np.inf;candidates.append(face)
 points=np.stack(candidates);dist2=np.sum((points-center)**2,axis=2);which,triangle=np.unravel_index(np.argmin(dist2),dist2.shape);point=points[which,triangle];distance=float(np.sqrt(dist2[which,triangle]));source='structural-collision.glb' if triangle<g['records'][0]['triangles'] else 'native-railing-collision.glb'
 return dict(center=center.tolist(),radius=.3,nearestPoint=point.tolist(),distance=distance,distanceMinusRadius=distance-.3,triangle=int(triangle),source=source,contactNormal=((center-point)/distance).tolist())
for frame in [60,120,150,180,210,220,230,240,250,270,300,330,360,390,410,450,510,540]:
 record=next(f for f in d['routeFrames'] if f['frame']==frame);hero=next(c for c in record['characters'] if c['node']=='hero');pos=np.array(hero['position']);origin=pos+np.array([0,3,0]);t=origin-a;u=np.einsum('ij,ij->i',t,p)*inverse;q=np.cross(t,e1);v=np.einsum('j,ij->i',direction,q)*inverse;distance=np.einsum('ij,ij->i',e2,q)*inverse;hit=valid&(u>=0)&(v>=0)&(u+v<=1)&(distance>=0)&(distance<=6);ids=np.flatnonzero(hit);ids=ids[np.argsort(distance[ids])];hits=[dict(point=(origin+direction*distance[i]).tolist(),triangle=int(i),footMinusSurfaceHeight=float(pos[1]-(origin[1]-distance[i]))) for i in ids[:3]];rows.append(dict(frame=frame,position=pos.tolist(),grounded=hero['grounded'],verticalRayHits=hits,bottomSphere=sphere_contact(pos)))
report=dict(status='measured',sources=g['records'],preCaptureGeometryReportSha256=frozen,queries=rows,scope='Downward center rays against the two declared collision meshes explain planter surface heights. Capsule contact can use a leading or side surface; center rays do not replace the physics solver or prove native collision equivalence.')
(r/'contact-geometry.json').write_bytes((json.dumps(report,indent=2)+'\n').encode());print(json.dumps([dict(frame=x['frame'],footHeight=x['position'][1],sphereDistance=x['bottomSphere']['distance'],contactHeight=x['bottomSphere']['nearestPoint'][1],normalY=x['bottomSphere']['contactNormal'][1]) for x in rows],indent=2))
