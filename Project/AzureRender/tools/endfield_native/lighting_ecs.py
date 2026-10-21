"""Read current fixed ECS light columns and their serialized type-2 properties.

Offsets derive from UnityPlayer HGSharedLightData getters and component lookup
masks 42/63. Blob hydration (+8 for component 42) is a static-layout candidate;
live registration, state activation, radiometry and additional fields need audit.
"""
import argparse
import hashlib
import io
import json
import math
from pathlib import Path
import struct


def decode_buffers(common, light):
    if len(common)!=236 or len(light)!=448:
        raise ValueError('Unsupported ECS light buffer sizes')
    def values(raw,offset,fmt):
        result=list(struct.unpack_from('<'+fmt,raw,offset))
        if not all(math.isfinite(x) for x in result):
            raise ValueError('Nonfinite ECS light semantic field')
        return result
    def scalar(raw,offset):return values(raw,offset,'f')[0]
    matrix=values(common,108,'16f')
    if any(abs(matrix[i])>1e-5 for i in (3,7,11)) or abs(matrix[15]-1)>1e-5:
        raise ValueError('Invalid ECS light affine matrix')
    result=dict(light_type=struct.unpack_from('<i',light,12)[0],
        shape=struct.unpack_from('<i',light,16)[0],color=values(common,0,'4f'),
        specular_intensity=scalar(light,20),
        intensity=scalar(common,16),color_temperature=scalar(common,20),
        range=scalar(light,24),spot_angle=scalar(light,36),
        inner_spot_angle=scalar(light,40),linear_length=scalar(light,144),
        falloff_exponent=scalar(light,444),
        obb_relative_position=values(light,100,'3f'),
        obb_half_extents=values(light,112,'3f'),
        obb_orientation=values(light,124,'3f'),
        obb_falloff_threshold=scalar(light,136),
        matrix=matrix,position=matrix[12:15],direction=matrix[8:11])
    for key,raw,offset in (('use_color_temperature',common,24),('enable_obb',light,92),
            ('is_sun_source',light,90),('shadow_only',light,91)):
        if raw[offset] not in (0,1):raise ValueError('Invalid ECS light boolean: '+key)
        result[key]=bool(raw[offset])
    if result['intensity']<0 or result['range']<0 or result['specular_intensity']<0:
        raise ValueError('Negative ECS light intensity, specular intensity or range')
    return result


def render_falloff(exponent):
    """Map the compiled punctual falloff branches to the renderer contract."""
    if not math.isfinite(exponent) or exponent==0 or exponent>64:
        raise ValueError('Unsupported native light falloff exponent')
    return dict(falloff='smooth-inverse-square' if exponent<0 else 'range-power',
                falloffExponent=2 if exponent<0 else exponent)


def render_influence_box(fields):
    """Convert inverse TRS and half packing used by LightCulling.PrepareCPUData.

    Source box translation and Euler orientation are relative world coordinates.
    The deferred shader subtracts light position before applying the packed rows.
    Unity Euler order is Z, X, Y, with angles in degrees.
    """
    if not fields['enable_obb']:return {'enabled':False}
    position=fields['position'];offset=fields['obb_relative_position']
    extents=fields['obb_half_extents'];angles=fields['obb_orientation']
    threshold=fields['obb_falloff_threshold']
    if not all(math.isfinite(x) for x in [*position,*offset,*extents,*angles,threshold]):
        raise ValueError('Nonfinite native influence box')
    if min(extents)<=0 or not 0<=threshold<=1:
        raise ValueError('Unsupported native influence box extents or threshold')
    x,y,z=[math.radians(a) for a in angles]
    sx,cx=math.sin(x),math.cos(x);sy,cy=math.sin(y),math.cos(y);sz,cz=math.sin(z),math.cos(z)
    # R = Ry * Rx * Rz. Inverse rows are transposed rotation / half extent.
    rotation=[[cy*cz+sy*sx*sz,-cy*sz+sy*sx*cz,sy*cx],
              [cx*sz,cx*cz,-sx],[-sy*cz+cy*sx*sz,sy*sz+cy*sx*cz,cy*cx]]
    rows=[]
    try:
        half=lambda f:struct.unpack('<e',struct.pack('<e',f))[0]
        for i in range(3):
            row=[rotation[j][i]/extents[i] for j in range(3)]
            translation=-sum(a*b for a,b in zip(row,offset))
            row=[half(a) for a in [*row,translation]]
            row[3]-=sum(a*b for a,b in zip(row,position))
            rows.append(row)
    except (OverflowError,struct.error) as error:
        raise ValueError('Native influence box exceeds half precision') from error
    determinant=sum(rows[0][i]*(rows[1][(i+1)%3]*rows[2][(i+2)%3]-rows[1][(i+2)%3]*rows[2][(i+1)%3]) for i in range(3))
    if not math.isfinite(determinant) or determinant==0:
        raise ValueError('Packed native influence box is singular')
    return dict(enabled=True,worldToBox=rows,falloffThreshold=threshold)


def merge_rows(rows):
    unique={}
    for row in rows:
        previous=unique.get(row['entity_id'])
        if previous:
            if previous['identity']!=row['identity']:
                raise ValueError('conflicting ECS light entity: '+str(row['entity_id']))
            sources={json.dumps(x,sort_keys=True):x for x in previous['sources']+row['sources']}
            previous['sources']=[sources[key] for key in sorted(sources)]
        else:unique[row['entity_id']]=dict(row)
    return [unique[key] for key in sorted(unique)]


def _properties(data,table,fb):
    ids,n=fb.vector_from_field(data,table,3)
    types,nt=fb.vector_from_field(data,table,4)
    values,nv=fb.vector_from_field(data,table,5)
    if n!=nt or n!=nv:raise ValueError('ECS property vector counts differ')
    result={}
    sizes={1:1,2:4,3:4,4:4,9:12,10:16,15:8}
    for j in range(n):
        key=fb.u32(data,ids+j*4);kind=data[types+j]
        target=fb.offset_target(data,values+j*4)
        if key in result:raise ValueError('Duplicate ECS property ID')
        if kind==17:
            inner=fb.offset_target(data,fb.field_address(data,target,0))
            start,length=fb.vector_from_field(data,inner,0)
        elif kind in sizes:start,length=target,sizes[kind]
        else:raise ValueError(f'Unsupported ECS property type: {kind}, id={key:#x}')
        if start<0 or start+length>len(data):raise ValueError('ECS property exceeds source')
        result[key]=(kind,data[start:start+length])
    return result


def _ecs_record(data,record,fb):
    if record.type_tag!=2:raise ValueError('Light requires serialized ECS type 2')
    table=record.start;name_address=fb.field_address(data,table,0)
    if name_address is None:raise ValueError('ECS record name field missing')
    start=fb.offset_target(data,name_address);length=fb.u32(data,start)
    if start+4+length>len(data):raise ValueError('ECS record name exceeds source')
    name=data[start+4:start+4+length].decode('utf8')
    header=tuple(fb.u32(data,fb.field_address(data,table,i)) for i in (1,2))
    props_at=fb.field_address(data,table,3)
    if props_at is None:raise ValueError('ECS properties missing')
    props=_properties(data,fb.offset_target(data,props_at),fb)
    return name,header,props


def _light_columns(data,fb):
    root=fb.u32(data,0);ids,ni=fb.vector_from_field(data,root,6)
    groups,ng=fb.vector_from_field(data,root,7)
    if ni!=ng:raise ValueError('ECS parallel group counts differ')
    for gi in range(ng):
        table=fb.table_vector_element(data,groups,gi)
        count=fb.u32(data,fb.field_address(data,table,1))
        ds,nd=fb.vector_from_field(data,table,3)
        blob,length=fb.wrapped_vector(data,fb.offset_target(data,fb.field_address(data,table,4)))
        offsets={};cursor=0
        for j in range(nd):
            kind,stride,reserved=struct.unpack_from('<HHI',data,ds+j*8)
            if reserved or not stride or kind in offsets:raise ValueError('Invalid ECS column descriptor')
            offsets[kind]=(cursor,stride);cursor+=count*stride
        if cursor!=length:raise ValueError('ECS column blob length differs')
        if 42 not in offsets and 63 not in offsets:continue
        if any(offsets.get(k,(0,0))[1]!=size for k,size in ((21,64),(42,244),(63,448))):
            raise ValueError('Unsupported ECS light columns')
        entity_ids,nids=fb.wrapped_vector(data,fb.table_vector_element(data,ids,gi))
        if nids!=count:raise ValueError('ECS light entity count differs')
        for ri in range(count):
            columns={k:data[blob+o+ri*st:blob+o+(ri+1)*st] for k,(o,st) in offsets.items()}
            name=columns[21].split(b'\0')[0].decode('utf8')
            yield fb.u32(data,entity_ids+4*ri),gi,ri,name,columns


def read_ecs_lights(vfs_root,scene,path_hash=None):
    from extractor_core import resources,vfs,flatbuffer as fb
    from extractor_core.projected_decals import streaming_records,build_path_id_index
    from extractor_core.scene_identity import validate_scene_id
    from endfield_native.scene import decode_chunk_data
    validate_scene_id(scene);group_hash='C3442D43'
    group=resources.load_group(vfs_root,group_hash)
    entries={e.file_name:e for e in group.iter_files()}
    prefix=f'Data/Streaming/PC/{scene}/Streaming/InitChunkData_'
    sources={};rows=[];groups=set();physical=0
    paths=build_path_id_index(path_hash) if path_hash else {}
    def read(path):
        entry=entries[path];output=io.BytesIO()
        with (vfs_root/group_hash/(entry.file_chunk_md5_name+'.chk')).open('rb') as source:
            digest=vfs.copy_virtual_file(source,output,entry,group.version,bytes.fromhex(vfs.DEFAULT_VFS_KEY_HEX))
        if digest!=entry.file_data_md5.upper():raise ValueError('ECS light VFS MD5 mismatch')
        encoded=output.getvalue();data=decode_chunk_data(encoded)
        sources[path]=dict(md5=digest,encoded_sha256=hashlib.sha256(encoded).hexdigest(),
            decoded_sha256=hashlib.sha256(data).hexdigest(),bytes=len(data))
        return data
    for init_path in sorted(p for p in entries if p.startswith(prefix) and p.endswith('.bytes')):
        init=read(init_path);columns=list(_light_columns(init,fb))
        if not columns:continue
        stream_path=init_path.replace('/InitChunkData_','/StreamingChunkData_')
        stream=read(stream_path);ir,sr=streaming_records(init),streaming_records(stream)
        for entity,gi,ri,name,template in columns:
            a=ir.get(entity);b=sr.get(entity)
            if a is None or b is None:raise ValueError('ECS light serialized partner missing')
            iname,ih,ip=_ecs_record(init,a,fb);sname,sh,sp=_ecs_record(stream,b,fb)
            if name!=iname or not name or sname or ih!=sh:
                raise ValueError('ECS light source identity differs')
            for key,size in ((0xff000000,236),(0xff010000,448)):
                if key not in ip or ip[key][0]!=17 or len(ip[key][1])!=size:
                    raise ValueError('ECS light required serialized buffer missing')
            fields=decode_buffers(ip[0xff000000][1],ip[0xff010000][1])
            raw_props=lambda ps:[dict(id=hex(k),type=t,raw_hex=v.hex(),path_matches=list(paths.get(v,())) if t==15 else []) for k,(t,v) in sorted(ps.items())]
            raw=dict(init_properties=raw_props(ip),streaming_properties=raw_props(sp),
                column_templates={str(k):v.hex() for k,v in template.items()})
            # Name and every source property participate; repeated spatial tiles
            # may use different placeholder columns, which remain per occurrence.
            identity=hashlib.sha256(json.dumps([name,ih,raw['init_properties'],raw['streaming_properties']],sort_keys=True).encode()).hexdigest()
            rows.append(dict(entity_id=entity,name=name,identity=identity,fields=fields,
                init_properties=raw['init_properties'],streaming_properties=raw['streaming_properties'],
                sources=[dict(init=init_path,streaming=stream_path,group=gi,index=ri,column_templates=raw['column_templates'])]))
            groups.add((init_path,gi));physical+=1
    result=dict(format='AzureEndfieldEcsLights/1',scene=scene,
        catalog_sha256=hashlib.sha256((vfs_root/group_hash/(group_hash+'.blc')).read_bytes()).hexdigest(),
        column_group_count=len(groups),physical_record_count=physical,rows=merge_rows(rows),source_sha256=sources,
        limitations=['static getter-array association and blob hydration require live confirmation',
            'state activation, additional fields, cookie use and radiometric equivalence remain unverified'])
    if path_hash:result['path_hash_sha256']=hashlib.sha256(path_hash.read_bytes()).hexdigest()
    return result


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--vfs-root',type=Path,required=True);p.add_argument('--scene',required=True)
    p.add_argument('--path-hash',type=Path);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args()
    if a.output.exists() or a.output.resolve().is_relative_to(a.vfs_root.resolve()):
        raise ValueError('Output must be a new path outside the game')
    result=read_ecs_lights(a.vfs_root,a.scene,a.path_hash)
    a.output.write_text(json.dumps(result,indent=2,allow_nan=False),encoding='utf8')
    print(json.dumps({k:result[k] for k in ('column_group_count','physical_record_count')},ensure_ascii=False), 'unique',len(result['rows']))


if __name__=='__main__':main()
