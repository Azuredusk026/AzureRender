"""Check direct resource-pointer operations in the four compiled unlit fog programs."""
import argparse
import json
from pathlib import Path
import struct


def check(path):
    data=path.read_bytes()
    if len(data)<20 or len(data)%4:raise ValueError('Invalid SPIR-V length: '+str(path))
    words=struct.unpack('<'+str(len(data)//4)+'I',data)
    if words[0]!=0x07230203:raise ValueError('Invalid SPIR-V magic: '+str(path))
    bindings={};operations=[];cursor=5
    while cursor<len(words):
        length=words[cursor]>>16;opcode=words[cursor]&65535
        if length<1 or cursor+length>len(words):raise ValueError('Invalid SPIR-V instruction')
        args=words[cursor+1:cursor+length]
        if opcode==71 and len(args)==3 and args[1]==33:bindings[args[0]]=args[2]
        if opcode in (61,65,66,67,68,83) and len(args)>=3:operations.append(args[2])
        elif opcode==62 and args:operations.append(args[0])
        elif opcode==57 and len(args)>3:operations.extend(args[3:])
        cursor+=length
    used={binding for variable,binding in bindings.items() if variable in operations}
    if not {0,27,34,35}<=used:raise AssertionError(f'{path.name}: missing fog frame, profile or camera operations: {sorted(used)}')
    if used&{23,24,32,33}:raise AssertionError(f'{path.name}: unlit fog accesses unrelated reflection or irradiance resources: {sorted(used)}')
    return dict(program=path.name,directlyUsedBindings=sorted(used))


def run(directory):
    names=['mesh_fog.frag.spv','mesh_fog_bindless.frag.spv','mesh_fog_samples.frag.spv','mesh_fog_bindless_samples.frag.spv']
    return dict(status='passed',programs=[check(directory/name) for name in names],
                scope='Direct loads, access chains, copies, array lengths, stores and function pointer arguments; no general control-flow reflection.')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('shader_directory',type=Path)
    args=parser.parse_args();print(json.dumps(run(args.shader_directory),indent=2))
