"""Compile versioned shader requests and validate reflection against host ABI."""
from dataclasses import dataclass
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile

SLANG_VERSION = '2026.8'
IDENTIFIER = re.compile(r'[A-Za-z_]\w*\Z')


def layout_types(description):
    if set(description) != {'schemaVersion', 'types'} or type(description['schemaVersion']) is not int or description['schemaVersion'] != 1:
        raise ValueError('Unsupported layout description fields or version')
    types = {}
    for value in description['types']:
        if set(value) != {'name','size','alignment','fields'} or not IDENTIFIER.fullmatch(value['name']):
            raise ValueError('Invalid layout type fields')
        fields = value['fields']
        if value['name'] in types or not 1 <= len(fields) <= 32:
            raise ValueError('Invalid layout type identity or size')
        if type(value['alignment']) is not int or type(value['size']) is not int or value['alignment'] != 4 or value['size'] != len(fields)*4:
            raise ValueError('Layout scalar structure requires four-byte alignment and exact span')
        names = set()
        for index,field in enumerate(fields):
            if set(field) != {'name','type','offset','default'} or not IDENTIFIER.fullmatch(field['name']):
                raise ValueError('Invalid layout member fields')
            if field['name'] in names or type(field['offset']) is not int or field['offset'] != index*4 or field['type'] not in {'float32','uint32'}:
                raise ValueError('Layout member identity, offset or scalar type mismatch')
            names.add(field['name'])
            default = field['default']
            if isinstance(default,bool) or not isinstance(default,(int,float)) or not math.isfinite(default):
                raise ValueError('Layout defaults require finite scalars')
            if field['type']=='uint32' and (not isinstance(default,int) or not 0<=default<=0xffffffff):
                raise ValueError('Layout uint32 default exceeds range')
            if field['type']=='float32' and abs(default)>3.402823466e38:
                raise ValueError('Layout float32 default exceeds range')
        types[value['name']] = value
    if not types:
        raise ValueError('Layout description requires a type')
    return types


@dataclass(frozen=True)
class ShaderCompileRequest:
    source: Path
    entry: str
    target: str
    profile: str
    compiler: Path
    include_roots: tuple
    layout: Path
    defines: dict
    output: Path

    @classmethod
    def parse(cls,value):
        allowed={'schemaVersion','source','entry','target','profile','compiler','includeRoots','layout','defines','output'}
        if set(value)!=allowed or type(value['schemaVersion']) is not int or value['schemaVersion']!=1:
            raise ValueError('Unsupported shader request fields or version')
        if value['target']!='spirv' or value['profile']!='spirv_1_3' or not IDENTIFIER.fullmatch(value['entry']):
            raise ValueError('Unsupported shader target, profile or entry')
        if not isinstance(value['defines'],dict) or any(not IDENTIFIER.fullmatch(k)
            or not re.fullmatch(r'[A-Za-z0-9_.+-]{1,64}',v) for k,v in value['defines'].items()):
            raise ValueError('Invalid shader define fields')
        roots=tuple(Path(p).resolve() for p in value['includeRoots'])
        if not roots or any(not p.is_dir() for p in roots):
            raise ValueError('Shader include roots require existing directories')
        return cls(Path(value['source']).resolve(),value['entry'],value['target'],value['profile'],
                   Path(value['compiler']).resolve(),roots,Path(value['layout']).resolve(),value['defines'],Path(value['output']).resolve())


def dependencies(source,roots):
    result,active={},set()
    def visit(path):
        path=path.resolve()
        if path in active: raise ValueError('Shader dependency cycle: '+str(path))
        if path in result: return
        if len(active)>=64 or len(result)>=1024: raise ValueError('Shader dependency budget exceeded')
        if not any(path.is_relative_to(root) for root in roots) or not path.is_file():
            raise ValueError('Shader dependency is outside declared roots or missing: '+str(path))
        data=path.read_bytes()
        if len(data)>2*1024*1024: raise ValueError('Shader dependency byte budget exceeded')
        active.add(path)
        code=re.sub(r'/\*.*?\*/|//[^\n]*','',data.decode('utf-8-sig'),flags=re.S)
        for name in re.findall(r'^\s*import\s+([\w.]+)\s*;',code,re.M):
            relative=Path(*name.split('.')).with_suffix('.slang')
            found=next((root/relative for root in roots if (root/relative).is_file()),None)
            if found is None: raise ValueError('Missing shader dependency: '+name)
            visit(found)
        for name in re.findall(r'^\s*#include\s+"([^"\n]+)"',code,re.M):
            found=next((root/name for root in (path.parent,*roots) if (root/name).is_file()),None)
            if found is None: raise ValueError('Missing shader dependency: '+name)
            visit(found)
        active.remove(path)
        result[path]=hashlib.sha256(data).hexdigest()
    visit(source)
    return result


def spirv_layout(bytes_,types):
    if len(bytes_)<20 or len(bytes_)%4 or bytes_[:4]!=b'\x03\x02\x23\x07':
        raise ValueError('Compiler produced invalid SPIR-V')
    words=struct.unpack('<'+'I'*(len(bytes_)//4),bytes_)
    names,member_names,offsets,strides,type_members,scalar_types,arrays={},{},{},{},{},{},{}
    cursor=5
    def string(data): return struct.pack('<'+'I'*len(data),*data).split(b'\0',1)[0].decode('utf-8')
    while cursor<len(words):
        count,opcode=words[cursor]>>16,words[cursor]&0xffff
        if count==0 or cursor+count>len(words): raise ValueError('Malformed SPIR-V instruction')
        args=words[cursor+1:cursor+count]
        if opcode==5: names[args[0]]=string(args[1:])
        elif opcode==6: member_names[args[0],args[1]]=string(args[2:])
        elif opcode==72 and args[2]==35: offsets[args[0],args[1]]=args[3]
        elif opcode==71 and args[1]==6: strides[args[0]]=args[2]
        elif opcode==30: type_members[args[0]]=args[1:]
        elif opcode in {28,29}: arrays[args[0]]=args[1]
        elif opcode==22 and args[1]==32: scalar_types[args[0]]='float32'
        elif opcode==21 and args[1:]==(32,0): scalar_types[args[0]]='uint32'
        cursor+=count
    validated,structured=set(),{}
    for id_,name in names.items():
        canonical=next((key for key in types if name in {key,key+'_std430',key+'_std140'}),None)
        if canonical is None or id_ not in type_members: continue
        expected=types[canonical]
        actual=type_members[id_]
        if len(actual)!=len(expected['fields']): raise ValueError('SPIR-V layout member count mismatch')
        for index,field in enumerate(expected['fields']):
            if member_names.get((id_,index))!=field['name'] or offsets.get((id_,index))!=field['offset'] or scalar_types.get(actual[index])!=field['type']:
                raise ValueError('SPIR-V layout offset or scalar type mismatch: '+canonical)
        for array,element in arrays.items():
            if element==id_:
                if strides.get(array)!=expected['size']: raise ValueError('SPIR-V layout array stride mismatch')
                structured[canonical]=strides[array]
        validated.add(canonical)
    if validated!=set(types): raise ValueError('Declared layout types are missing from SPIR-V')
    return sorted(validated),structured


def validate_reflection(reflection,types):
    found=set()
    def visit(value):
        if isinstance(value,list):
            for child in value: visit(child)
        elif isinstance(value,dict):
            if value.get('kind')=='struct' and value.get('name') in types:
                expected=types[value['name']]['fields'];actual=value.get('fields',[])
                if len(actual)!=len(expected): raise ValueError('Reflection layout member count mismatch')
                for field,spec in zip(actual,expected):
                    if field.get('name')!=spec['name'] or field.get('binding',{}).get('offset')!=spec['offset'] or field.get('binding',{}).get('size')!=4 or field.get('type',{}).get('scalarType')!=spec['type']:
                        raise ValueError('Reflection layout offset or scalar type mismatch')
                found.add(value['name'])
            for child in value.values(): visit(child)
    visit(reflection)
    if found!=set(types): raise ValueError('Declared layout types are missing from reflection')


@dataclass(frozen=True)
class ShaderCompileResult:
    output: Path
    metadata: dict


def compile_module(request):
    if not request.compiler.is_file(): raise ValueError('Shader compiler is missing: '+str(request.compiler))
    version_result=subprocess.run([str(request.compiler),'-version'],capture_output=True,text=True,timeout=10,check=True)
    version=(version_result.stdout+version_result.stderr).strip()
    if version!=SLANG_VERSION: raise ValueError('Shader compiler requires Slang '+SLANG_VERSION+', got '+version)
    types=layout_types(json.loads(request.layout.read_text(encoding='utf-8')))
    inputs=dependencies(request.source,request.include_roots)
    inputs[request.layout]=hashlib.sha256(request.layout.read_bytes()).hexdigest()
    compiler_files=[request.compiler,*sorted(request.compiler.parent.glob('slang*.dll'))]
    compiler_artifacts={str(p.resolve()):hashlib.sha256(p.read_bytes()).hexdigest() for p in compiler_files}
    metadata_path=request.output.with_suffix(request.output.suffix+'.json')
    protected=set(inputs)|{Path(p) for p in compiler_artifacts}|{Path(__file__).resolve()}
    if request.output.resolve() in protected or metadata_path.resolve() in protected:
        raise ValueError('Shader output paths must be distinct from all inputs')
    signature=dict(inputs={str(p):h for p,h in inputs.items()},compiler=hashlib.sha256(request.compiler.read_bytes()).hexdigest(),
                   compilerArtifacts=compiler_artifacts,
                   adapterHash=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),floatingPointMode='precise',
                   compilerVersion=version,entry=request.entry,target=request.target,profile=request.profile,defines=request.defines,
                   includeRoots=[str(p) for p in request.include_roots])
    fingerprint=hashlib.sha256(json.dumps(signature,sort_keys=True).encode()).hexdigest()
    if request.output.is_file() and metadata_path.is_file():
        previous=json.loads(metadata_path.read_text(encoding='utf-8'))
        if previous.get('inputHash')==fingerprint and previous.get('outputHash')==hashlib.sha256(request.output.read_bytes()).hexdigest():
            validate_reflection(previous.get('reflection',{}),types)
            spirv_layout(request.output.read_bytes(),types)
            previous['cacheHit']=True
            with tempfile.TemporaryDirectory(prefix='azure shader cache ',dir=request.output.parent) as folder:
                candidate=Path(folder)/'metadata.json'
                candidate.write_text(json.dumps(previous,indent=2)+'\n',encoding='utf-8')
                os.replace(candidate,metadata_path)
            return ShaderCompileResult(request.output,previous)
    request.output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='azure shader candidate ',dir=request.output.parent) as folder:
        candidate=Path(folder)/'candidate.spv';reflection_path=Path(folder)/'reflection.json'
        command=[str(request.compiler),str(request.source),'-entry',request.entry,'-target',request.target,'-profile',request.profile,
                 '-fp-mode','precise','-reflection-json',str(reflection_path),'-o',str(candidate)]
        for root in request.include_roots: command.extend(['-I',str(root)])
        command.extend('-D'+name+'='+value for name,value in sorted(request.defines.items()))
        result=subprocess.run(command,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=60)
        if result.returncode: raise ValueError('Shader compiler failed: '+result.stdout+result.stderr)
        reflected=json.loads(reflection_path.read_text(encoding='utf-8'))
        validate_reflection(reflected,types)
        binary=candidate.read_bytes();validated,strides=spirv_layout(binary,types)
        # Read snapshots again before atomically installing the validated output.
        if any(hashlib.sha256(p.read_bytes()).hexdigest()!=h for p,h in inputs.items()):
            raise ValueError('Shader dependency changed during compilation')
        if any(hashlib.sha256(Path(p).read_bytes()).hexdigest()!=h for p,h in compiler_artifacts.items()):
            raise ValueError('Shader compiler artifacts changed during compilation')
        metadata=dict(schemaVersion=1,**signature,inputHash=fingerprint,outputHash=hashlib.sha256(binary).hexdigest(),
                      cacheHit=False,validatedTypes=validated,structuredStrides=strides,reflection=reflected,diagnostics=result.stdout+result.stderr)
        temporary_metadata=Path(folder)/'metadata.json'
        temporary_metadata.write_text(json.dumps(metadata,indent=2)+'\n',encoding='utf-8')
        # Publication is serialized by the caller. Restore the previous binary
        # if installing its metadata fails; this is exception recovery, not a
        # filesystem-wide atomic pair or crash-consistency guarantee.
        backup=Path(folder)/'previous.spv'
        if request.output.exists():
            shutil.copy2(request.output,backup)
        os.replace(candidate,request.output)
        try:
            os.replace(temporary_metadata,metadata_path)
        except OSError:
            if backup.exists():
                os.replace(backup,request.output)
            else:
                request.output.unlink()
            raise
    return ShaderCompileResult(request.output,metadata)


def main():
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--request',type=Path,required=True);args=parser.parse_args()
    try:
        result=compile_module(ShaderCompileRequest.parse(json.loads(args.request.read_text(encoding='utf-8'))))
        print(json.dumps(dict(status='passed',output=str(result.output),cacheHit=result.metadata['cacheHit'])));return 0
    except (ValueError,OSError,KeyError,TypeError,subprocess.SubprocessError) as error:
        print(str(error),file=__import__('sys').stderr);return 1


if __name__=='__main__':
    raise SystemExit(main())
