"""Prepare a bounded demo copy, retaining UVs and normals through collapse mapping.
Requires fast-simplification==0.2.0 (MIT); it is an offline content tool.
"""
import argparse,json,importlib.metadata
from pathlib import Path
import numpy as np
import fast_simplification as fs
def simplify(path,reduction=.5):
    path=Path(path);d=json.loads(path.read_text(encoding='utf-8'));buffers=[(path.parent/b['uri']).read_bytes() for b in d['buffers']];out=bytearray();before=after=0
    sizes={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4};types={5121:np.uint8,5123:np.dtype('<u2'),5125:np.dtype('<u4'),5126:np.dtype('<f4')}
    def read(index):
        a=d['accessors'][index];v=d['bufferViews'][a['bufferView']];dtype=types[a['componentType']];n=sizes[a['type']];offset=v.get('byteOffset',0)+a.get('byteOffset',0);stride=v.get('byteStride',np.dtype(dtype).itemsize*n)
        return np.ndarray((a['count'],n),dtype=dtype,buffer=buffers[v['buffer']],offset=offset,strides=(stride,np.dtype(dtype).itemsize)).copy()
    def add(data,kind):
        if kind=='SCALAR':data=data.reshape(-1,1)
        while len(out)%4:out.append(0)
        view=len(d['bufferViews']);payload=data.tobytes();d['bufferViews'].append({'buffer':len(d['buffers']),'byteOffset':len(out),'byteLength':len(payload)});out.extend(payload)
        a={'bufferView':view,'componentType':5125 if kind=='SCALAR' else 5126,'count':len(data),'type':kind}
        if kind=='VEC3':a.update(min=data.min(axis=0).tolist(),max=data.max(axis=0).tolist())
        d['accessors'].append(a);return len(d['accessors'])-1
    for mesh in d['meshes']:
        for p in mesh['primitives']:
            original=read(p['attributes']['POSITION']).astype(np.float64);faces=read(p['indices']).reshape(-1,3).astype(np.int32);before+=len(faces)
            if len(faces)<200:after+=len(faces);continue
            points,triangles,collapses=fs.simplify(original,faces,target_reduction=reduction,return_collapses=True,preserve_border=True)
            replayed,replayedFaces,mapping=fs.replay_simplification(original,faces,collapses)
            p['indices']=add(replayedFaces.astype('<u4'),'SCALAR');after+=len(replayedFaces)
            for semantic,index in list(p['attributes'].items()):
                if semantic=='POSITION':values=replayed
                else:
                    old=read(index);values=np.zeros((len(replayed),old.shape[1]));count=np.bincount(mapping,minlength=len(replayed));np.add.at(values,mapping,old);values/=np.maximum(count[:,None],1)
                    if semantic in ('NORMAL','TANGENT'):values[:,:3]/=np.maximum(np.linalg.norm(values[:,:3],axis=1)[:,None],1e-9)
                    if semantic=='TANGENT':values[:,3]=np.where(values[:,3]<0,-1,1)
                p['attributes'][semantic]=add(np.asarray(values,dtype='<f4'),d['accessors'][index]['type'])
    filename='azure-demo-simplified.bin';(path.parent/filename).write_bytes(out);d['buffers'].append({'uri':filename,'byteLength':len(out)})
    path.write_text(json.dumps(d,indent=2),encoding='utf-8')
    receipt={'tool':'fast-simplification','version':importlib.metadata.version('fast-simplification'),'targetReduction':reduction,'sourceMeshTriangles':before,'outputMeshTriangles':after,'attributes':'collapse-map weighted interpolation; preserve border'}
    print(json.dumps(receipt));return receipt
if __name__=='__main__':
    p=argparse.ArgumentParser(__doc__);p.add_argument('gltf',type=Path);p.add_argument('--reduction',type=float,default=.5);a=p.parse_args();simplify(a.gltf,a.reduction)
