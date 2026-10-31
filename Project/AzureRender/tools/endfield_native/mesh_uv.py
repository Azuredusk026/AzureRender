"""Restore source primary UVs across explicitly matching mesh topology."""
import copy
import numpy as np


def _integer(value,minimum=0):
    if type(value) is not int or value<minimum:
        raise ValueError('Invalid mesh accessor integer')
    return value


def _read(document,binary,index,kind,components):
    index=_integer(index);a=document['accessors'][index]
    if a.get('type')!=kind or _integer(a.get('componentType')) not in components or 'sparse' in a:
        raise ValueError('Unsupported mesh accessor type or sparse layout')
    if type(a.get('normalized',False)) is not bool or a.get('normalized',False):
        raise ValueError('Normalized mesh accessors are unsupported')
    view=document['bufferViews'][_integer(a['bufferView'])]
    if type(view.get('buffer')) is not int or view['buffer']!=0:
        raise ValueError('Mesh UV restoration requires embedded buffer zero')
    if len(document['buffers'])!=1 or 'uri' in document['buffers'][0]:
        raise ValueError('Mesh UV restoration requires one embedded buffer')
    declared=_integer(document['buffers'][0]['byteLength'])
    if declared>len(binary):raise ValueError('Mesh binary is truncated')
    dtype=np.dtype({5126:'<f4',5125:'<u4',5123:'<u2',5121:'u1'}[a['componentType']])
    width={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[kind]
    count=_integer(a['count'],1);base=_integer(view.get('byteOffset',0));offset=_integer(a.get('byteOffset',0));length=_integer(view['byteLength'])
    stride=_integer(view.get('byteStride',width*dtype.itemsize),width*dtype.itemsize)
    if base%dtype.itemsize or offset%dtype.itemsize or stride%dtype.itemsize:
        raise ValueError('Unaligned mesh accessor')
    if base+length>declared or offset+(count-1)*stride+width*dtype.itemsize>length:
        raise ValueError('Mesh accessor exceeds its buffer view')
    values=np.ndarray((count,width),dtype=dtype,buffer=binary,offset=base+offset,
        strides=(stride,dtype.itemsize)).copy()
    if not np.isfinite(values).all():raise ValueError('Nonfinite mesh attribute')
    return values


def _primitives(document,selected):
    from convert_material_semantics import slots as material_slots
    identities=material_slots(document);result={slot:[] for slot in selected}
    if not set(selected)<=set(identities):raise ValueError('Selected material slot is missing')
    for mesh in document['meshes']:
        for primitive in mesh['primitives']:
            index=_integer(primitive.get('material'))
            if index>=len(identities):raise ValueError('Primitive material index is invalid')
            if identities[index] in result:result[identities[index]].append(primitive)
    if any(not primitives for primitives in result.values()):raise ValueError('Selected material slot has no primitives')
    return result


def restore_primary_uv(document,binary,reference,reference_binary,selected_slots):
    """Copy source UV0 using exact indexed topology and matching tangent signs.

    Callers establish source vertex identity. Pose-dependent positions and
    tangent XYZ can differ. Existing tangent streams remain authored inputs.
    All original bytes and unselected primitive references are preserved.
    """
    try:
        if document.get('asset',{}).get('version')!='2.0' or reference.get('asset',{}).get('version')!='2.0':
            raise ValueError('Mesh UV restoration requires glTF version 2.0')
        if not isinstance(selected_slots,(list,tuple)) or not selected_slots or any(type(s) is not str or not s for s in selected_slots):
            raise ValueError('Explicit material slots are required')
        if len(set(selected_slots))!=len(selected_slots):raise ValueError('Duplicate selected material slot')
        if not isinstance(binary,(bytes,bytearray)) or not isinstance(reference_binary,(bytes,bytearray)):
            raise ValueError('Mesh buffers must contain bytes')
        result=copy.deepcopy(document);targets=_primitives(result,selected_slots);sources=_primitives(reference,selected_slots)
        payload=bytearray(binary);report=dict(vertices=0,primitives=[],materialSlots=list(selected_slots))
        for slot in selected_slots:
            if len(targets[slot])!=len(sources[slot]):raise ValueError('Source primitive count differs')
            for target,source in zip(targets[slot],sources[slot]):
                mode=_integer(target.get('mode',4));ref_mode=_integer(source.get('mode',4))
                if mode>6 or ref_mode>6 or mode!=ref_mode:raise ValueError('Source primitive mode differs or is invalid')
                target['attributes']=dict(target['attributes'])
                attrs=target['attributes'];ref_attrs=source['attributes']
                uv=_read(result,binary,attrs['TEXCOORD_0'],'VEC2',(5126,));expected=_read(reference,reference_binary,ref_attrs['TEXCOORD_0'],'VEC2',(5126,))
                position=_read(result,binary,attrs['POSITION'],'VEC3',(5126,));ref_position=_read(reference,reference_binary,ref_attrs['POSITION'],'VEC3',(5126,))
                count=len(uv)
                if len(position)!=count or len(ref_position)!=count or len(expected)!=count:raise ValueError('Source vertex count differs')
                indices=_read(result,binary,target['indices'],'SCALAR',(5121,5123,5125));ref_indices=_read(reference,reference_binary,source['indices'],'SCALAR',(5121,5123,5125))
                if not np.array_equal(indices,ref_indices) or indices.max()>=count:raise ValueError('Source index topology differs')
                if ('TANGENT' in attrs)!=('TANGENT' in ref_attrs):raise ValueError('Source tangent presence differs')
                if 'TANGENT' in attrs:
                    tangent=_read(result,binary,attrs['TANGENT'],'VEC4',(5126,));ref_tangent=_read(reference,reference_binary,ref_attrs['TANGENT'],'VEC4',(5126,))
                    if len(tangent)!=count or len(ref_tangent)!=count or not np.array_equal(tangent[:,3],ref_tangent[:,3]) or not np.all(np.abs(tangent[:,3])==1):
                        raise ValueError('Source tangent sign differs')
                payload.extend(b'\0'*(-len(payload)%4));offset=len(payload);data=expected.astype('<f4').tobytes();payload.extend(data)
                result['bufferViews'].append(dict(buffer=0,byteOffset=offset,byteLength=len(data),target=34962))
                result['accessors'].append(dict(bufferView=len(result['bufferViews'])-1,componentType=5126,count=count,type='VEC2',min=expected.min(0).tolist(),max=expected.max(0).tolist()))
                attrs['TEXCOORD_0']=len(result['accessors'])-1
                report['vertices']+=count;report['primitives'].append(dict(slot=slot,vertices=count,indices=len(indices),tangentSignVerified='TANGENT' in attrs))
        result['buffers'][0]['byteLength']=len(payload)
        return result,bytes(payload),report
    except (KeyError,IndexError,TypeError,AttributeError,OverflowError) as error:
        raise ValueError('Invalid mesh UV source structure: '+str(error)) from error


def main():
    import argparse
    import hashlib
    import json
    from pathlib import Path
    import struct
    import sys
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
    from convert_material_semantics import read_asset,write_asset
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ['input','reference','output','report']:parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--slot',action='append',required=True)
    args=parser.parse_args()
    protected={args.input.resolve(),args.reference.resolve()};outputs=[args.output.resolve(),args.report.resolve()]
    if len(set(outputs))!=2 or any(p in protected or p.exists() for p in outputs):
        raise ValueError('Output and report must be distinct new files outside the sources')
    def load(path):
        if path.suffix.lower()!='.glb':raise ValueError('An embedded GLB source is required')
        document,tail=read_asset(path)
        if tail is None or len(tail)<8:raise ValueError('GLB binary chunk is missing')
        size,kind=struct.unpack_from('<II',tail)
        if kind!=0x004e4942 or size%4 or 8+size>len(tail):raise ValueError('Invalid GLB binary chunk')
        cursor=8+size
        while cursor<len(tail):
            if cursor+8>len(tail):raise ValueError('Truncated GLB extra chunk')
            length,extra=struct.unpack_from('<II',tail,cursor)
            if extra==0x004e4942 or length%4 or cursor+8+length>len(tail):raise ValueError('Invalid GLB extra chunk')
            cursor+=8+length
        return document,tail[8:8+size],tail[8+size:]
    document,binary,extra=load(args.input);reference,reference_binary,_=load(args.reference)
    result,payload,report=restore_primary_uv(document,binary,reference,reference_binary,args.slot)
    payload+=b'\0'*(-len(payload)%4)
    tail=struct.pack('<II',len(payload),0x004e4942)+payload+extra
    args.output.parent.mkdir(parents=True,exist_ok=True)
    write_asset(args.output,result,tail)
    decoded,decoded_tail=read_asset(args.output)
    if decoded!=result or decoded_tail!=tail or payload[:len(binary)]!=binary:
        raise AssertionError('Restored GLB round trip or binary prefix differs')
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    report.update(status='source-uv-restored',sourceSha256=sha(args.input),referenceSha256=sha(args.reference),
        outputSha256=sha(args.output),generatorSha256=sha(Path(__file__)),originalBinaryPrefixPreserved=True)
    args.report.parent.mkdir(parents=True,exist_ok=True)
    args.report.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report))


if __name__=='__main__':main()
