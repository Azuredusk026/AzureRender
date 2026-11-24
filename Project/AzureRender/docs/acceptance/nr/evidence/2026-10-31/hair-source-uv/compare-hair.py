from pathlib import Path
import json,sys,hashlib,numpy as np
root=Path.cwd();r=Path(__file__).resolve().parent;sys.path.insert(0,str(root/'tools'));from retarget_animation import read_glb
paths=[root/'build/runs/20261031-135416-character-portrait-recovery-0773ca/original-reference/project/native-character/outline-converted.glb',root/'build/runs/20261031-164808-character-frontal-lighting-fa3879/project/content-character/hero-restored-inputs-v7.glb'];assets=[]
def accessor(d,b,index):
 a=d['accessors'][index];v=d['bufferViews'][a['bufferView']];dtype={5126:'<f4',5125:'<u4',5123:'<u2',5121:'u1'}[a['componentType']];width={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4,'MAT4':16}[a['type']];return np.ndarray((a['count'],width),dtype=dtype,buffer=b,offset=v.get('byteOffset',0)+a.get('byteOffset',0),strides=(v.get('byteStride',np.dtype(dtype).itemsize*width),np.dtype(dtype).itemsize)).copy()
for path in paths:
 d,chunks=read_glb(path);binary=next(b for k,b in chunks if k==0x004e4942);material=next(i for i,m in enumerate(d['materials']) if m.get('name')=='M_actor_laevat_hair_01');p=next(p for m in d['meshes'] for p in m['primitives'] if p.get('material')==material);a={name:accessor(d,binary,index) for name,index in p['attributes'].items()};a['INDICES']=accessor(d,binary,p['indices']);assets.append((d,binary,a))
rows=[]
for key in sorted(set(assets[0][2])|set(assets[1][2])):
 a=assets[0][2].get(key);b=assets[1][2].get(key)
 row=dict(attribute=key,referenceShape=None if a is None else a.shape,currentShape=None if b is None else b.shape)
 if a is not None and b is not None and a.shape==b.shape:row.update(equal=bool(np.array_equal(a,b)),maximumAbsoluteDifference=float(np.max(np.abs(a.astype(float)-b.astype(float)))),referenceArraySha256=hashlib.sha256(a.tobytes()).hexdigest(),currentArraySha256=hashlib.sha256(b.tobytes()).hexdigest())
 rows.append(row)
report=dict(status='measured',scope='Direct original mesh element order; no positional remapping or tolerance-based matching. Attribute equality does not prove visible texel equality.',sources=[dict(path=str(p.relative_to(root)),sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in paths],attributes=rows);(r/'hair-geometry-inputs.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
