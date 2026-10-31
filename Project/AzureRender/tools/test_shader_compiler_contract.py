"""Run real mixed shader compilation and recovery in an owned directory."""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import sys


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable',type=Path,required=True)
    parser.add_argument('--config',type=Path,required=True)
    parser.add_argument('--dxc',type=Path,required=True)
    args=parser.parse_args();engine=Path(__file__).resolve().parents[1]
    run=Path(subprocess.check_output([sys.executable,str(engine/'tools/artifact_cleanup.py'),'--new-run','shader-compiler-tests'],cwd=engine,text=True).strip())
    report=dict(status='running',executableSha256=hashlib.sha256(args.executable.read_bytes()).hexdigest(),configSha256=hashlib.sha256(args.config.read_bytes()).hexdigest())
    try:
        command=[str(args.executable.resolve()),str(args.config.resolve()),str(args.dxc.resolve()),str(run/'work')]
        result=subprocess.run(command,cwd=engine,capture_output=True,timeout=90)
        (run/'compiler.log').write_bytes(result.stdout+result.stderr);report['command']=command;report['exitCode']=result.returncode
        assert result.returncode==0,(result.stdout+result.stderr).decode('utf-8',errors='replace')
        child=json.loads((run/'work/result.json').read_text(encoding='utf-8'));assert child['status']=='passed';report['result']=child
        report['outputs']=[dict(file=str(p.relative_to(run)),bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in sorted((run/'work/verified').glob('*.spv'))]
        assert len(report['outputs'])==3;report['status']='passed'
        subprocess.run([sys.executable,str(engine/'tools/artifact_cleanup.py'),'--finish',str(run)],check=True)
        print('Shader compiler contract passed:',run)
    except Exception as error:report.update(status='failed',error=str(error));raise
    finally:(run/'summary.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':main()
