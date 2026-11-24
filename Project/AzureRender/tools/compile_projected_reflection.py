"""Compile the public projected reflection program and record its provenance."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    source=Path(__file__).resolve().parents[1]/'shaders/projected_reflection_trace.hlsl'
    args.output.mkdir();binary=args.output/'projected_reflection_trace.spv'
    compiler=args.sdk/'Bin/dxc.exe';validator=args.sdk/'Bin/spirv-val.exe'
    command=[str(compiler),'-spirv','-T','cs_6_0','-E','main','-HV','2018','-fspv-target-env=vulkan1.1',
        '-fvk-b-shift','0','0','-fvk-t-shift','1','0','-fvk-s-shift','8','0','-fvk-u-shift','9','0','-Od','-Fo',str(binary),str(source)]
    report=dict(status='failed',command=command)
    try:
        for name,cmd in (('compile',command),('validate',[str(validator),'--target-env','vulkan1.1',str(binary)])):
            result=subprocess.run(cmd,capture_output=True,timeout=60)
            (args.output/(name+'.log')).write_bytes(result.stdout+result.stderr)
            report[name+'ExitCode']=result.returncode
            if result.returncode:raise RuntimeError(name+' failed')
        report.update(status='passed',files={p.name:dict(bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest())
            for p in (source,binary,compiler,compiler.with_name('dxcompiler.dll'),validator)})
    finally:(args.output/'source.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('Public projected reflection compiled:',binary)


if __name__=='__main__':main()
