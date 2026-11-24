"""Read observed native reflection records without assigning unverified semantics.

Run through run_upstream.py for authenticated VFS access. Component and field
IDs are observations from game 1.0.14. Float27 is retained as a coefficient
candidate; its basis, units and activation require independent source evidence.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct


def decode_probe_fields(fields):
    def floats(key, kind, count):
        entry = fields.get(key)
        if entry is None or entry[0] != kind or len(entry[1]) != count * 4:
            raise ValueError(f'Probe field type/length differs: {key:#x}')
        values = list(struct.unpack('<'+'f'*count, entry[1]))
        if not all(math.isfinite(v) for v in values):
            raise ValueError(f'Nonfinite probe field: {key:#x}')
        return values
    result = {'raw_fields': {hex(k): {'type': t, 'hex': v.hex()}
                             for k, (t, v) in fields.items()},
              'runtime_activation': 'unverified', 'units': 'unverified',
              'coefficient_basis': 'unverified'}
    for name, key, kind, count in (
        ('size_candidate', 0x10000000, 9, 3),
        ('center_candidate', 0x10010000, 9, 3),
        ('matrix_a', 0x10040000, 17, 16),
        ('matrix_b', 0x10050000, 17, 16),
        ('coefficient_candidate', 0x100b0000, 17, 27),
        ('half_extent_candidate', 0x100f0000, 9, 3)):
        result[name] = None if key == 0x100b0000 and key not in fields else floats(key, kind, count)
    path = fields.get(0x100c0000)
    if path is None or path[0] != 16:
        raise ValueError('Missing typed probe texture path')
    result['texture_path'] = path[1].decode('utf8')
    if not result['texture_path'] or '\0' in result['texture_path']:
        raise ValueError('Invalid probe texture path')
    return result


def merge_records(rows):
    unique = {}
    for row in rows:
        key = (row['scene'], row['entity_id'])
        core = {k: v for k, v in row.items() if k not in ('source', 'source_sha256', 'stream_source', 'stream_sha256')}
        if key in unique:
            if unique[key][0] != core:
                raise ValueError(f'Conflicting native probe identity: {key}')
            unique[key][1]['sources'].append({k:v for k,v in row.items() if k not in core})
        else:
            unique[key] = (core, dict(core, sources=[{k:v for k,v in row.items() if k not in core}]))
    return [x[1] for x in unique.values()]


def _components(data, record):
    from extractor_core import flatbuffer as fb
    def window(at, count):
        if at < 0 or count < 0 or at + count > len(data):
            raise ValueError('Native probe payload exceeds source')
        return data[at:at+count]
    vec, count = fb.vector_from_field(data, record.start, 4)
    result = {}
    sizes = {1:1, 2:4, 3:4, 4:4, 6:8, 9:12, 10:16, 15:8}
    for index in range(count):
        table = fb.table_vector_element(data, vec, index)
        bit = struct.unpack('<Q', window(fb.field_address(data, table, 0), 8))[0]
        ids, n = fb.vector_from_field(data, table, 3)
        types, nt = fb.vector_from_field(data, table, 4)
        values, nv = fb.vector_from_field(data, table, 5)
        if n != nt or n != nv or bit in result:
            raise ValueError('Native probe component vectors differ')
        fields = {}
        for k in range(n):
            fid = fb.u32(data, ids+4*k)
            typ = window(types+k, 1)[0]
            at = fb.offset_target(data, values+4*k)
            if fid in fields:
                raise ValueError('Duplicate native probe field')
            if typ in sizes:
                raw = window(at, sizes[typ])
            elif typ == 16:
                target = fb.offset_target(data, fb.field_address(data, at, 0))
                raw = window(target+4, fb.u32(data, target))
            elif typ == 17:
                child = fb.offset_target(data, fb.field_address(data, at, 0))
                start, length = fb.vector_from_field(data, child, 0)
                raw = window(start, length)
            else:
                raise ValueError(f'Unsupported native probe field encoding: {typ}')
            fields[fid] = typ, raw
        result[bit] = fields
    return result


def read_reflections(vfs_root, scene):
    from extractor_core import resources, flatbuffer as fb
    from extractor_core.projected_decals import streaming_records
    from extractor_core.scene_identity import validate_scene_id
    from endfield_native.scene import decode_chunk_data
    validate_scene_id(scene)
    rows = []
    prefix = f'Data/Streaming/PC/{scene}/Streaming/InitChunkData_'
    for entry in resources.load_group(vfs_root, 'C3442D43').iter_files():
        if not entry.file_name.startswith(prefix):
            continue
        data = decode_chunk_data(resources.read_virtual_file(vfs_root, 'C3442D43', entry.file_name))
        stream = records = None
        for entity, rec in streaming_records(data).items():
            if rec.type_tag != 1:
                continue
            mask_at = fb.field_address(data, rec.start, 3)
            if mask_at is None or not struct.unpack_from('<Q', data, mask_at)[0] & 0x10000:
                continue
            components = _components(data, rec)
            transform = components.get(1, {})
            position = transform.get(0)
            if position is None or position[0] != 9 or len(position[1]) != 12:
                raise ValueError('Missing native probe transform')
            pos = list(struct.unpack('<3f', position[1]))
            if not all(math.isfinite(v) for v in pos):
                raise ValueError('Nonfinite native probe position')
            nf = fb.field_address(data, rec.start, 0)
            ns = fb.offset_target(data, nf)
            name = data[ns+4:ns+4+fb.u32(data, ns)].decode('utf8')
            sp = entry.file_name.replace('/InitChunkData_', '/StreamingChunkData_')
            if stream is None:
                stream = decode_chunk_data(resources.read_virtual_file(vfs_root, 'C3442D43', sp))
                records = streaming_records(stream)
            if entity not in records or records[entity].type_tag != 1:
                raise ValueError('Missing paired native probe record')
            streamed = _components(stream, records[entity])
            rows.append(dict(scene=scene, entity_id=entity, name=name, position=pos,
                probe=decode_probe_fields(components[0x10000]),
                raw_components={hex(b):{hex(k):{'type':t,'hex':v.hex()} for k,(t,v) in fs.items()} for b,fs in components.items()},
                streaming_components={hex(b):{hex(k):{'type':t,'hex':v.hex()} for k,(t,v) in fs.items()} for b,fs in streamed.items()},
                source=entry.file_name, source_sha256=hashlib.sha256(data).hexdigest(),
                stream_source=sp, stream_sha256=hashlib.sha256(stream).hexdigest()))
    return {'schema':'NativeReflectionInventory/1', 'scene':scene,
            'physical_record_count':len(rows), 'probes':merge_records(rows)}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--vfs-root',type=Path,required=True)
    parser.add_argument('--scene',required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if args.output.resolve().is_relative_to(args.vfs_root.resolve()):
        raise ValueError('Output must be outside source VFS')
    result=read_reflections(args.vfs_root,args.scene)
    with args.output.open('x',encoding='utf8') as target:
        json.dump(result,target,indent=2,allow_nan=False)
    print('Native reflection probes:',len(result['probes']))


if __name__=='__main__':main()
