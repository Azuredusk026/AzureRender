import hashlib,json,sys
from pathlib import Path
import numpy as np
root=Path.cwd();run=Path(__file__).resolve().parent;sys.path.insert(0,str(root/'tools'))
from retarget_animation import read_glb,trs_matrix
from endfield_native.capture_skin_frame import compare_skin_frame
dynamic=root/'build/runs/20261031-231334-receiver-directional-light-911ab7/project/content-character/hero-restored-inputs-v8.glb'
reference=root/'build/runs/20261031-135416-character-portrait-recovery-0773ca/original-reference/project/native-character/outline-converted.glb'
native=json.loads((root/'build/runs/20261022-212213-native-binding-restoration-d39ea8/frame10401/frame-bone-comparison.json').read_text())
class Mesh:
    def __init__(self,path):
        self.d,chunks=read_glb(path);self.b=next(b for kind,b in chunks if kind==0x004e4942)
        self.parents={c:i for i,n in enumerate(self.d['nodes']) for c in n.get('children',[])};self.worlds={}
    def data(self,index):
        a=self.d['accessors'][index];v=self.d['bufferViews'][a['bufferView']]
        dtype={5126:'<f4',5125:'<u4',5123:'<u2',5121:'u1'}[a['componentType']]
        width={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4,'MAT4':16}[a['type']];size=np.dtype(dtype).itemsize
        data=np.ndarray((a['count'],width),dtype=dtype,buffer=self.b,offset=v.get('byteOffset',0)+a.get('byteOffset',0),strides=(v.get('byteStride',size*width),size)).astype(float)
        if a.get('normalized') and a['componentType']!=5126:data/=np.iinfo(np.dtype(dtype)).max
        return data
    def world(self,index):
        if index not in self.worlds:
            n=self.d['nodes'][index]
            m=np.array(n['matrix']).reshape(4,4,order='F') if 'matrix' in n else trs_matrix([n.get('translation',[0,0,0]),n.get('rotation',[0,0,0,1]),n.get('scale',[1,1,1])])
            self.worlds[index]=(self.world(self.parents[index]) if index in self.parents else np.eye(4))@m
        return self.worlds[index]
    def primitive(self,name):
        material=next(i for i,m in enumerate(self.d['materials']) if m.get('name')==name)
        mesh=next(i for i,m in enumerate(self.d['meshes']) if any(p.get('material')==material for p in m['primitives']))
        node=next(i for i,n in enumerate(self.d['nodes']) if n.get('mesh')==mesh)
        p=next(p for p in self.d['meshes'][mesh]['primitives'] if p.get('material')==material)
        return node,p
def unit(v):return v/np.maximum(np.linalg.norm(v,axis=1),1e-30)[:,None]
def angular(a,b):return np.degrees(np.arccos(np.clip(np.sum(unit(a)*unit(b),axis=1),-1,1)))
a=Mesh(dynamic);b=Mesh(reference);flip=np.diag([-1.,1.,1.,1.])
native_world={x['bone']:flip@np.array(x['world'])@flip for x in native['comparisons']}
capture=root/'build/runs/20261022-212213-native-binding-restoration-d39ea8/frame10401'
geometry_path=root/'build/runs/20261017-000956-endfield-native-3caf73/laevat-static/model.geometry.json'
geometry=json.loads(geometry_path.read_text())
hair_mesh=next(m for m in geometry['meshes'] if m['name']=='S_actor_laevat_hair_01_lod0')
binds=[flip@np.array(b['inverse_bind_matrix_column_major']).reshape(4,4,order='F')@flip for b in hair_mesh['bones']]
hair_pose=compare_skin_frame((capture/'e950-ps-cb2-binding.bin').read_bytes(),(capture/'e804-skin-matrices.bin').read_bytes(),binds)
overlap=[]
for item,bone in zip(hair_pose['bones'],hair_mesh['bones']):
    name=bone['path'].split('/')[-1];world=flip@np.array(item['world'])@flip
    if name in native_world:overlap.append(float(np.max(np.abs(native_world[name]-world))))
    else:native_world[name]=world
(run/'hair-native-pose.json').write_text(json.dumps({'result':hair_pose,'names':[b['path'].split('/')[-1] for b in hair_mesh['bones']], 'overlapMaxError':max(overlap,default=0)},indent=2)+'\n')
origin=(flip@np.array(native['model']))[:3,3]
reports=[]
for name in ['M_actor_laevat_face_01','M_actor_laevat_hair_01']:
    node,p=a.primitive(name);refnode,q=b.primitive(name);attrs=p['attributes'];refattrs=q['attributes']
    skin=a.d['skins'][a.d['nodes'][node]['skin']];joint_names=[a.d['nodes'][i]['name'] for i in skin['joints']]
    inverse_bind=a.data(skin['inverseBindMatrices']).reshape(-1,4,4).transpose(0,2,1)
    indices=a.data(attrs['JOINTS_0']).astype(int);weights=a.data(attrs['WEIGHTS_0']);weights/=weights.sum(axis=1)[:,None]
    mapped=np.array([n in native_world for n in joint_names]);valid=(weights*(~mapped[indices])).sum(axis=1)<1e-7
    assert np.array_equal(a.data(p['indices']),b.data(q['indices']))
    palette=np.array([native_world[n]@ib if n in native_world else np.eye(4) for n,ib in zip(joint_names,inverse_bind)])
    matrices=np.sum(palette[indices]*weights[:,:,None,None],axis=1)
    pos=a.data(attrs['POSITION']);predicted=np.einsum('nij,nj->ni',matrices,np.c_[pos,np.ones(len(pos))])[:,:3]-origin
    refpos=b.data(refattrs['POSITION']);refworld=b.world(refnode)
    reference_positions=(np.c_[refpos,np.ones(len(refpos))]@refworld.T)[:,:3]
    normals=a.data(attrs['NORMAL']);posed_normals=np.einsum('nij,nj->ni',matrices[:,:3,:3],normals)
    reference_normals=b.data(refattrs['NORMAL'])@refworld[:3,:3].T
    tangent=a.data(attrs['TANGENT']);ref_tangent=b.data(refattrs['TANGENT'])
    posed_tangent=np.einsum('nij,nj->ni',matrices[:,:3,:3],tangent[:,:3]);reference_tangent=ref_tangent[:,:3]@refworld[:3,:3].T
    distance=np.linalg.norm(predicted-reference_positions,axis=1)
    row={'material':name,'vertices':len(pos),'mappedVertices':int(valid.sum()),'referenceMeshHasSkin':'skin' in b.d['nodes'][refnode],
        'sourceMeshNode':a.d['nodes'][node],'referenceMeshNode':b.d['nodes'][refnode],
        'unmappedJointNames':sorted({joint_names[i] for i,w in zip(indices.ravel(),weights.ravel()) if w>1e-7 and not mapped[i]})}
    if 'TEXCOORD_0' in attrs and 'TEXCOORD_0' in refattrs:
        uv=a.data(attrs['TEXCOORD_0']);refuv=b.data(refattrs['TEXCOORD_0']);row['uvMaximumError']=float(np.abs(uv-refuv).max())
    if valid.any():
        n=angular(posed_normals[valid],reference_normals[valid]);t=angular(posed_tangent[valid],reference_tangent[valid])
        row.update(positionMaxMetres=float(distance[valid].max()),positionMedianMetres=float(np.median(distance[valid])),
            normalMedianDegrees=float(np.median(n)),normalMaximumDegrees=float(n.max()),
            tangentMedianDegrees=float(np.median(t)),tangentMaximumDegrees=float(t.max()),
            tangentWExact=bool(np.array_equal(tangent[valid,3],ref_tangent[valid,3])))
    reports.append(row)
result={'status':'native-pose-comparison-measured','scope':'CPU reconstruction with captured face and hair bone palettes; only vertices with complete native bone coverage are compared. Morphs, source topology and position agreement determine eligibility for interpreting normal differences.',
    'dynamicSha256':hashlib.sha256(dynamic.read_bytes()).hexdigest(),'referenceSha256':hashlib.sha256(reference.read_bytes()).hexdigest(),'knownNativeBones':len(native_world),'cases':reports}
(run/'audit.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
