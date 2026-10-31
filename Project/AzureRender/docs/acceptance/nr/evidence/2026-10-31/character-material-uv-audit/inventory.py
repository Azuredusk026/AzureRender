from pathlib import Path
import json,sys,hashlib,numpy as np
root=Path.cwd();r=Path(__file__).resolve().parent;sys.path.insert(0,str(root/'tools'));from retarget_animation import read_glb;from endfield_native.mesh_uv import _read
paths=[root/'build/runs/20261031-135416-character-portrait-recovery-0773ca/original-reference/project/native-character/outline-converted.glb',root/'build/runs/20261031-172112-hair-source-inputs-14d199/project/content-character/hero-restored-inputs-v8.glb'];data=[]
for p in paths:
 d,chunks=read_glb(p);binary=next(b for kind,b in chunks if kind==0x004e4942);primitives={d['materials'][q['material']]['name']:q for m in d['meshes'] for q in m['primitives']};data.append((d,binary,primitives))
rows=[]
for name in data[0][2]:
 source=[(d,b,p[name]) for d,b,p in data];uv=[_read(d,b,p['attributes']['TEXCOORD_0'],'VEC2',(5126,)) for d,b,p in source];indices=[_read(d,b,p['indices'],'SCALAR',(5121,5123,5125)) for d,b,p in source];row=dict(material=name,referenceVertices=len(uv[0]),currentVertices=len(uv[1]),indicesEqual=bool(np.array_equal(*indices)))
 if uv[0].shape==uv[1].shape:
  reflected=uv[1].copy();reflected[:,1]=1-reflected[:,1];row.update(directMaximumUvDifference=float(np.abs(uv[0]-uv[1]).max()),verticalReflectionMaximumUvDifference=float(np.abs(uv[0]-reflected).max()))
  if all('TANGENT' in p['attributes'] for d,b,p in source):
   tangents=[_read(d,b,p['attributes']['TANGENT'],'VEC4',(5126,)) for d,b,p in source];row['matchingTangentWVertices']=int(np.sum(tangents[0][:,3]==tangents[1][:,3]));row['matchingReflectedTangentWVertices']=int(np.sum(tangents[0][:,3]==-tangents[1][:,3]))
 rows.append(row)
report=dict(status='source-coordinate-inventory',scope='Direct source vertex order only. Equal topology and UV orientation statistics do not prove texture image orientation; material and texture bindings require independent checks before restoration.',baseCommit='4188d66',sources=[dict(path=str(p.relative_to(root)),sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in paths],materials=rows);(r/'coordinate-inventory.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(rows,indent=2))
