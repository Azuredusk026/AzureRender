"""Compile and validate the fingerprinted native ray Vulkan reference."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from native_render.ray_vulkan_shader import reference_source,SOURCE_SHA256


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    engine=Path(__file__).resolve().parents[1]
    source=engine/'docs/acceptance/nr/evidence/2026-10-29/native-ray-source/ray-mirror-raw.hlsl'
    args.output.mkdir()
    path=args.output/'reference.hlsl';path.write_text(reference_source(source.read_bytes()),encoding='utf-8',newline='\n')
    binary=args.output/'reference.spv';compiler=args.sdk/'Bin/dxc.exe';validator=args.sdk/'Bin/spirv-val.exe'
    command=[str(compiler),'-spirv','-T','cs_6_0','-E','main','-HV','2018','-fspv-target-env=vulkan1.1',
        '-fvk-b-shift','0','0','-fvk-t-shift','3','0','-fvk-s-shift','10','0','-fvk-u-shift','11','0','-Od','-Fo',str(binary),str(path)]
    report=dict(status='failed',command=command,originalSourceSha256=SOURCE_SHA256)
    try:
        for name,cmd in (('compile',command),('validate',[str(validator),'--target-env','vulkan1.1',str(binary)])):
            result=subprocess.run(cmd,capture_output=True,timeout=60)
            (args.output/(name+'.log')).write_bytes(result.stdout+result.stderr)
            report[name+'ExitCode']=result.returncode
            if result.returncode:raise RuntimeError(name+' failed')
        report.update(status='passed',files={p.name:dict(bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest())
            for p in (source,path,binary,compiler,compiler.with_name('dxcompiler.dll'),validator)})
    finally:(args.output/'source.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('Reference compiled and validated:',binary)


if __name__=='__main__':main()
