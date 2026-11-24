"""Read lossless RenderDoc VSOut exports with packed float/uint signatures.

This offline reader consumes replay-owned layouts and indices. It does not
infer vertex identity from draw index count or assign game shader semantics.
"""
from pathlib import Path
import hashlib,json
import numpy as np

def read_postvs(path,instance=0):
    path=Path(path);root=path.parent.resolve();data=json.loads(path.read_text(encoding='utf-8'))
    if data.get('schema_version')!=1 or data.get('complete') is not True or data.get('stage')!='VSOut':
        raise ValueError('A complete version-one VSOut export is required')
    records=[r for r in data.get('instances',[]) if r.get('instance')==instance]
    if len(records)!=1:raise ValueError('A unique exported instance is required')
    record=records[0];mesh=record['mesh'];signature=data.get('output_signature',[])
    def number(key,minimum=0):
        value=mesh.get(key)
        if type(value) is not int or value<minimum:raise ValueError('Invalid VSOut layout: '+key)
        return value
    stride=number('vertexByteStride',1);offset=number('vertexByteOffset');count=number('numIndices',1)
    index_stride=number('indexByteStride',1);index_offset=number('indexByteOffset')
    base=mesh.get('baseVertex',0)
    if type(base) is not int or index_stride not in (1,2,4) or mesh.get('topology')!=5:
        raise ValueError('Only indexed triangle-list VSOut is supported')
    fmt=mesh.get('format',{})
    if fmt.get('compByteWidth')!=4 or fmt.get('compType')!=1 or fmt.get('type')!=0:
        raise ValueError('VSOut position requires regular float32 components')
    positions=[field for field in signature if field.get('semanticIdxName')=='SV_Position']
    if len(positions)!=1 or positions[0].get('compCount')!=4 or positions[0].get('varType')!=0:
        raise ValueError('VSOut requires one regular float32 position signature')
    # Replay places the position first even when the shader declares it last.
    signature=positions+[field for field in signature if field.get('semanticIdxName')!='SV_Position']
    layout=[];cursor=0;names=set()
    for field in signature:
        name=field.get('semanticIdxName');components=field.get('compCount')
        if not name or name in names or type(components) is not int or not 1<=components<=4:
            raise ValueError('Invalid or duplicate VSOut semantic')
        if field.get('varType') not in (0,4) or field.get('stream')!=0 or field.get('perPrimitiveRate',False):
            raise ValueError('Only per-vertex float32/uint32 stream-zero signatures are supported')
        names.add(name);layout.append((name,cursor,components,'<f4' if field['varType']==0 else '<u4'));cursor+=4*components
    if cursor!=stride:raise ValueError('Packed signature does not match replay vertex stride')
    def buffer(role):
        name=record.get('files',{}).get(role)
        if not isinstance(name,str) or not name:raise ValueError('Missing VSOut backing filename')
        entries=[b for b in data.get('buffers',[]) if b.get('file')==name and b.get('rid')==mesh.get(role+'ResourceId')]
        if len(entries)!=1:raise ValueError('Missing or ambiguous VSOut backing buffer')
        target=(root/name).resolve()
        if not target.is_relative_to(root):raise ValueError('Backing buffer escapes export directory')
        raw=target.read_bytes();entry=entries[0]
        if len(raw)!=entry.get('bytes') or hashlib.sha256(raw).hexdigest()!=entry.get('sha256'):
            raise ValueError('VSOut backing buffer digest or length mismatch')
        return raw
    vertices=buffer('vertex');indices=buffer('index')
    def bound(key,available):
        value=mesh.get(key,(1<<64)-1)
        if type(value) is not int or value<0:raise ValueError('Invalid byte range: '+key)
        return available if value==(1<<64)-1 else min(available,value)
    index_bytes=count*index_stride
    if index_offset>len(indices) or index_bytes>bound('indexByteSize',len(indices)-index_offset):
        raise ValueError('Draw indices exceed exported byte range')
    values=np.frombuffer(indices,dtype='<u'+str(index_stride),count=count,offset=index_offset).astype(np.int64)+base
    if (values<0).any():raise ValueError('Negative resolved VSOut vertex index')
    vertex_count=int(values.max())+1
    if offset>len(vertices) or vertex_count*stride>bound('vertexByteSize',len(vertices)-offset):
        raise ValueError('Draw references vertices outside exported byte range')
    attributes={name:np.ndarray((vertex_count,components),dtype=dtype,buffer=vertices,
        offset=offset+start,strides=(stride,4)).copy() for name,start,components,dtype in layout}
    return {'attributes':attributes,'indices':values,'mesh':mesh,'manifest':data}
