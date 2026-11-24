"""Restore GLB skin influences from a source FBX geometry audit."""
import argparse
from collections import defaultdict
import hashlib
import itertools
import json
from pathlib import Path
import struct
import numpy as np
from retarget_animation import read_glb

class SourceWeights:
    def __init__(self, source, tolerance=5e-7, flip_v=False):
        self.tolerance=tolerance; self.cells=defaultdict(list)
        for mesh in source['meshes']:
            for index,u,v in mesh['corners']:
                vertex=mesh['vertices'][index]; position=np.asarray(vertex['position'],dtype=float)
                influences={mesh['bones'][bone]:float(weight) for bone,weight in vertex['weights'] if weight>0}
                if not influences or not np.isfinite(list(influences.values())).all():
                    raise ValueError('Source vertex has invalid weights')
                total=sum(influences.values()); influences={bone:weight/total for bone,weight in influences.items()}
                uv=np.asarray([u,1-v if flip_v else v]); key=tuple(np.floor(position/tolerance).astype(int))
                self.cells[key].append((position,uv,influences))

    def match(self, position, uv):
        position=np.asarray(position); uv=np.asarray(uv);key=np.floor(position/self.tolerance).astype(int)
        candidates=[]
        for offset in itertools.product((-1,0,1),repeat=3):
            for p,source_uv,weights in self.cells.get(tuple(key+offset),[]):
                # Unreal glTF UVs originate in a half-float vertex buffer.
                if np.linalg.norm(p-position)<=self.tolerance and np.linalg.norm(source_uv-uv)<=7e-4:
                    candidates.append(weights)
        if not candidates: raise ValueError(f'No source match at position {position.tolist()}, UV {uv.tolist()}')
        reference=candidates[0]
        if any(set(w)!=set(reference) or any(abs(w[b]-reference[b])>2e-4 for b in reference) for w in candidates[1:]):
            raise ValueError(f'Ambiguous source weights at {position.tolist()}')
        return reference

def values(document,binary,index):
    a=document['accessors'][index];v=document['bufferViews'][a['bufferView']]
    dtype={5126:'<f4',5123:'<u2',5121:'u1'}[a['componentType']];count={'VEC2':2,'VEC3':3,'VEC4':4}[a['type']]
    return np.ndarray((a['count'],count),dtype=dtype,buffer=binary,offset=v.get('byteOffset',0)+a.get('byteOffset',0),
        strides=(v.get('byteStride',np.dtype(dtype).itemsize*count),np.dtype(dtype).itemsize))

def main():
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--target',type=Path,required=True)
    parser.add_argument('--source',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--flip-v',action='store_true');args=parser.parse_args()
    if args.output.exists():raise FileExistsError(args.output)
    document,chunks=read_glb(args.target);original=chunks[1][1];binary=bytearray(original)
    source=SourceWeights(json.loads(args.source.read_text(encoding='utf-8')),flip_v=args.flip_v)
    bone_map={document['nodes'][node]['name'].replace(' ','_'):i for i,node in enumerate(document['skins'][0]['joints'])}
    if len(bone_map)!=len(document['skins'][0]['joints']):raise ValueError('Ambiguous target bone names')
    restored=0;maximum_discarded=0
    def append(array,component):
        binary.extend(b'\0'*(-len(binary)%4));offset=len(binary);binary.extend(array.tobytes())
        view=len(document['bufferViews']);document['bufferViews'].append({'buffer':0,'byteOffset':offset,'byteLength':array.nbytes})
        accessor=len(document['accessors']);document['accessors'].append({'bufferView':view,'componentType':component,'count':len(array),'type':'VEC4'})
        return accessor
    for mesh in document['meshes']:
        for primitive in mesh['primitives']:
            attrs=primitive['attributes']
            if 'JOINTS_0' not in attrs:continue
            positions=values(document,original,attrs['POSITION']);uvs=values(document,original,attrs['TEXCOORD_0'])
            joints=np.zeros((len(positions),4),dtype='<u2');weights=np.zeros((len(positions),4),dtype='<f4')
            for i,(position,uv) in enumerate(zip(positions,uvs)):
                influences=source.match(position,uv)
                ranked=sorted(influences.items(),key=lambda item:(-item[1],item[0]));chosen=ranked[:4]
                maximum_discarded=max(maximum_discarded,sum(w for _,w in ranked[4:]));total=sum(w for _,w in chosen)
                for slot,(bone,weight) in enumerate(chosen):
                    joints[i,slot]=bone_map[bone.split(':')[-1].replace(' ','_')];weights[i,slot]=weight/total
            attrs['JOINTS_0']=append(joints,5123);attrs['WEIGHTS_0']=append(weights,5126);restored+=len(positions)
    report={'targetSha256':hashlib.sha256(args.target.read_bytes()).hexdigest(),
        'sourceSha256':hashlib.sha256(args.source.read_bytes()).hexdigest(),'verticesRestored':restored,
        'maximumDiscardedWeight':maximum_discarded,'positionToleranceMeters':source.tolerance,'uvTolerance':7e-4,'flipV':args.flip_v}
    document['asset'].setdefault('extras',{})['azureSkinRestore']=report;document['buffers'][0]['byteLength']=len(binary)
    payload=json.dumps(document,separators=(',',':')).encode();payload+=b' '*(-len(payload)%4);binary.extend(b'\0'*(-len(binary)%4))
    content=struct.pack('<II',len(payload),0x4e4f534a)+payload+struct.pack('<II',len(binary),0x004e4942)+binary
    for kind,chunk in chunks[2:]:content+=struct.pack('<II',len(chunk),kind)+chunk
    args.output.write_bytes(struct.pack('<III',0x46546c67,2,len(content)+12)+content)
    report['outputSha256']=hashlib.sha256(args.output.read_bytes()).hexdigest()
    args.output.with_suffix('.skin.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(report))

if __name__=='__main__':main()
