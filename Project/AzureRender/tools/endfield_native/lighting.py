"""Read native paired light records using the pinned upstream field IDs.

Field contracts: Endfield-map-extractor 6226108267942ec940cc65b6850ea8c49198c20f,
extractor_core/lights.py (MIT). Supports observed 46-field and upstream
60-field layouts. Full raw properties and paired-source hashes are retained.
Invoke through run_upstream.py for authenticated VFS compatibility.
"""
import argparse
from dataclasses import asdict
import json
import math
from pathlib import Path
import struct


def decode_light_fields(fields):
    if len(fields) not in (46,60):
        raise ValueError('Unsupported native light schema size')
    specs = [('light_type',0,2,'i'),('shape',1,2,'i'),('color',2,10,'4f'),
             ('intensity',3,4,'f'),('range',4,4,'f'),('spot_angle',5,4,'f'),
             ('linear_length',6,4,'f'),('inner_spot_angle',7,4,'f'),
             ('color_temperature',21,4,'f'),('use_color_temperature',22,1,'B'),
             ('enable_obb',26,1,'B')]
    result = {}
    for name,slot,kind,fmt in specs:
        entry = fields.get(0x11000000 + slot*65536)
        if entry is None or entry[0] != kind or len(entry[1]) != struct.calcsize('<'+fmt):
            raise ValueError('Native light field type/size differs: '+name)
        values = struct.unpack('<'+fmt,entry[1])
        if not all(math.isfinite(v) for v in values):
            raise ValueError('Nonfinite native light: '+name)
        if kind == 1 and values[0] not in (0,1):
            raise ValueError('Invalid native light boolean')
        result[name] = values if len(values)>1 else (bool(values[0]) if kind==1 else values[0])
    if result['range'] < 0 or result['intensity'] < 0:
        raise ValueError('Negative light range or intensity')
    return result


def read_lights(vfs_root, path_hash, scene):
    from extractor_core import lights, resources
    from endfield_native.scene import validate_raw_header

    def raw(root, entry):
        data = resources.read_virtual_file(root,'C3442D43',entry.file_name)
        validate_raw_header(data)
        return data

    def row(map_id, entity_id, name, mask, components, cookie_path, init_path, streaming_path):
        transform, light, additional = (components[k] for k in (1,131072,33554432))
        if len(transform)!=3 or len(additional)!=21:
            raise ValueError('Unsupported light transform/additional schema')
        fields = decode_light_fields(light)
        return lights.NativeLight(map_id=map_id,entity_id=entity_id,name=name,
            position=lights._vector(transform,0,9,init_path),
            euler_zxy=lights._vector(transform,65536,9,init_path),
            scale=lights._vector(transform,131072,9,init_path),**fields,
            character_only=lights._bool(additional,0x19100000,init_path),
            falloff_exponent=lights._float(additional,0x19120000,init_path),
            cookie_path=cookie_path,component_mask=mask,
            light_properties=lights._raw_items(light),additional_properties=lights._raw_items(additional),
            source_init_paths=(init_path,),source_streaming_paths=(streaming_path,),
            baseline_sha256=lights._fingerprint(name,mask,components))
    old_read,old_row = lights.read_chunk,lights._light_row
    try:
        lights.read_chunk,lights._light_row = raw,row
        return lights.read_native_lights(vfs_root,path_hash,scene,None)
    finally:
        lights.read_chunk,lights._light_row = old_read,old_row


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('vfs-root','path-hash','output'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--scene',required=True)
    args=parser.parse_args()
    if args.output.resolve().is_relative_to(args.vfs_root.resolve()):
        raise ValueError('Output must be outside source VFS')
    result=read_lights(args.vfs_root,args.path_hash,args.scene)
    with args.output.open('x',encoding='utf8') as out:
        json.dump(asdict(result),out,indent=2,allow_nan=False)
    print('Native lights:',len(result.rows))


if __name__=='__main__':main()
