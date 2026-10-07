"""Measure full, cached and dependency-edit shader compilation on one host."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
from compile_shader_module import ShaderCompileRequest,compile_module

ROOT=Path(__file__).resolve().parents[1]


def main():
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--compiler',type=Path,required=True)
    parser.add_argument('--glslc',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    args.output.mkdir(parents=True,exist_ok=True);rows=[]
    with tempfile.TemporaryDirectory(prefix='azure shader comparison ') as temporary:
        folder=Path(temporary);modules=folder/'modules';shutil.copytree(ROOT/'shaders/modules',modules)
        for round_ in range(3):
            destination=folder/f'glsl-{round_}.spv';start=time.perf_counter()
            result=subprocess.run([str(args.glslc.resolve()),str(ROOT/'shaders/bloom_downsample.comp'),'-o',str(destination)],
                capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=60)
            assert result.returncode==0,result.stderr
            rows.append(dict(language='glsl',round=round_+1,kind='full',milliseconds=(time.perf_counter()-start)*1000,
                bytes=destination.stat().st_size,sha256=hashlib.sha256(destination.read_bytes()).hexdigest()))
            for name,mode in [('direct','0'),('cached','1')]:
                destination=folder/f'{name}-{round_}.spv'
                request=ShaderCompileRequest.parse(dict(schemaVersion=1,source=str(modules/'Bloom.slang'),entry='main',target='spirv',
                    profile='spirv_1_3',compiler=str(args.compiler.resolve()),includeRoots=[str(modules)],layout=str(modules/'shared-layout.json'),
                    defines={'AZURE_BLOOM_CACHE':mode},output=str(destination)))
                for kind in ['full','unchanged','dependency-edit']:
                    if kind=='dependency-edit':
                        source=modules/'strategies/BlockStrategies.slang'
                        source.write_text(source.read_text()+f'\n// measured dependency edit {name} {round_}\n')
                    start=time.perf_counter();result=compile_module(request);elapsed=(time.perf_counter()-start)*1000
                    assert result.metadata['cacheHit']==(kind=='unchanged')
                    rows.append(dict(language='slang',strategy=name,round=round_+1,kind=kind,milliseconds=elapsed,
                        bytes=destination.stat().st_size,sha256=result.metadata['outputHash'],inputHash=result.metadata['inputHash'],cacheHit=result.metadata['cacheHit']))
    report=dict(schemaVersion=1,status='passed',rounds=rows,variants=2,entryPoints=1,sharedAlgorithms=1,samplingImplementations=2,
        compilerSha256=hashlib.sha256(args.compiler.read_bytes()).hexdigest(),glslCompilerSha256=hashlib.sha256(args.glslc.read_bytes()).hexdigest(),
        comparison='Same input algorithm; GLSL direct compiler and checked Slang adapter; unchanged build invokes no GLSL compiler')
    (args.output/'compilation.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(status='passed',measurements=len(rows),variants=2)))


if __name__=='__main__':main()
