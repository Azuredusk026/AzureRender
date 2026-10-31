"""Validate physical mirror hits and qualified coordinate/motion controls."""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import sys


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable',type=Path,required=True)
    parser.add_argument('--shader',type=Path,required=True)
    args=parser.parse_args();engine=Path(__file__).resolve().parents[1]
    run=Path(subprocess.check_output([sys.executable,str(engine/'tools/artifact_cleanup.py'),'--new-run','projected-camera-gpu'],cwd=engine,text=True).strip())
    report=dict(status='running',cases=[],shaderSha256=hashlib.sha256(args.shader.read_bytes()).hexdigest(),executableSha256=hashlib.sha256(args.executable.read_bytes()).hexdigest())
    try:
        for mode,success in (('positive',True),('wide-edge',True),('history-static',True),('nonfinite-motion',True),('wrong-convention',False),('wrong-motion',False)):
            output=run/mode
            command=[str(args.executable.resolve()),str(args.shader.resolve()),str(output),mode]
            result=subprocess.run(command,cwd=engine,capture_output=True,timeout=60)
            (run/(mode+'.log')).write_bytes(result.stdout+result.stderr)
            child=json.loads((output/'result.json').read_text(encoding='utf-8'))
            report['cases'].append(dict(mode=mode,exitCode=result.returncode,result=child,command=command))
            assert child['validationErrors']==child['liveBuffers']==child['liveImages']==0,child
            assert child['regionPixels']==2432 and child['minimumFraction']==.75,child
            if success:assert result.returncode==0 and child['status']=='passed',child
            else:assert result.returncode!=0 and child['status']=='failed',child
        baseline=(run/'positive/2.bin').read_bytes();wide=(run/'wide-edge/2.bin').read_bytes()
        assert len(baseline)==len(wide)==320*240 and all(b<=a for a,b in zip(baseline,wide))
        different=sum(a!=b for a,b in zip(baseline,wide));assert different>0
        assert all((run/'positive'/name).read_bytes()==(run/'wide-edge'/name).read_bytes() for name in ('0.bin','1.bin'))
        report['edgeSensitivity']=dict(confidenceDifferentBytes=different,monotonic=True,colorExact=True,footprintExact=True)
        report['status']='passed'
        subprocess.run([sys.executable,str(engine/'tools/artifact_cleanup.py'),'--finish',str(run)],check=True)
        print('Projected camera GPU cases passed:',len(report['cases']),run)
    except Exception as error:report.update(status='failed',error=str(error));raise
    finally:(run/'summary.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':main()
