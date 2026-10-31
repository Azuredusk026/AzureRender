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
    parser.add_argument('--preparation',type=Path)
    parser.add_argument('--composition',type=Path)
    parser.add_argument('--motion-contract',action='store_true')
    args=parser.parse_args();engine=Path(__file__).resolve().parents[1]
    if args.preparation and args.composition:parser.error('Choose preparation or composition')
    if args.motion_contract and not args.composition:parser.error('Motion contract requires composition controls')
    run=Path(subprocess.check_output([sys.executable,str(engine/'tools/artifact_cleanup.py'),'--new-run','projected-camera-gpu'],cwd=engine,text=True).strip())
    report=dict(status='running',cases=[],shaderSha256=hashlib.sha256(args.shader.read_bytes()).hexdigest(),executableSha256=hashlib.sha256(args.executable.read_bytes()).hexdigest())
    if args.preparation:report['preparationSha256']=hashlib.sha256(args.preparation.read_bytes()).hexdigest()
    if args.composition:report['compositionDirectory']=str(args.composition.resolve())
    try:
        cases=[('positive',True),('wide-edge',True),('history-static',True),('nonfinite-motion',True)]
        if args.motion_contract:
            cases=[('history-response-tracked',True),('history-response-assumed',True),('history-response-cutout',True),('history-response-partial',True),('response-current',True),('history-offset',True),('history-wrong-offset',False)]
            cases.extend((name,False) for name in ('history-response-background','history-response-missing','history-response-reactive','history-response-nonfinite','history-response-depth','history-response-zero-depth','history-response-outside'))
        elif args.composition:cases.extend([('history-tinted',True),('history-shifted',True),('history-wrong-pose',False),('history-disoccluded',False)])
        else:cases.extend([('wrong-convention',False),('wrong-motion',False)])
        for mode,success in cases:
            output=run/mode
            command=[str(args.executable.resolve()),str(args.shader.resolve()),str(output),mode]
            if args.preparation:command.append(str(args.preparation.resolve()))
            if args.composition:command.extend(['--composition',str(args.composition.resolve())])
            result=subprocess.run(command,cwd=engine,capture_output=True,timeout=60)
            (run/(mode+'.log')).write_bytes(result.stdout+result.stderr)
            child=json.loads((output/'result.json').read_text(encoding='utf-8'))
            report['cases'].append(dict(mode=mode,exitCode=result.returncode,result=child,command=command))
            assert child['validationErrors']==child['liveBuffers']==child['liveImages']==0,child
            assert child['regionPixels']==2432 and child['minimumFraction']==.75,child
            assert child['minimumConfidence']==(32 if mode in ('history-response-cutout','history-response-partial') else 128),child
            assert child['surfacePreparation']==bool(args.preparation or args.composition),child
            if args.composition:
                assert child['composition']=='public',child
                if success:assert child['filteredRedHits']*4>=child['regionPixels']*3,child
                control=run/(mode+'-control');control_command=command[:-2]+['--control',str(args.composition.resolve())];control_command[2]=str(control)
                reference=subprocess.run(control_command,cwd=engine,capture_output=True,timeout=60)
                (run/(mode+'-control.log')).write_bytes(reference.stdout+reference.stderr)
                control_result=json.loads((control/'result.json').read_text(encoding='utf-8'))
                assert reference.returncode==(0 if success else 1) and control_result['validationErrors']==control_result['liveBuffers']==control_result['liveImages']==0,control_result
                differences=[]
                for i in range(4):
                    actual=(output/f'{i}.bin').read_bytes();expected=(control/f'{i}.bin').read_bytes()
                    assert len(actual)==len(expected)
                    differences.append(dict(output=i,bytes=len(actual),differentBytes=sum(a!=b for a,b in zip(actual,expected)),sha256=hashlib.sha256(actual).hexdigest()))
                report['cases'][-1]['independentControl']=dict(command=control_command,result=control_result,outputs=differences)
                assert all(row['differentBytes']==0 for row in differences),differences
            if success:assert result.returncode==0 and child['status']=='passed',child
            else:assert result.returncode!=0 and child['status']=='failed',child
        if args.motion_contract:
            mode='history-response-mixed';output=run/mode
            command=[str(args.executable.resolve()),str(args.shader.resolve()),str(output),mode]
            result=subprocess.run(command,cwd=engine,capture_output=True,timeout=60);(run/(mode+'.log')).write_bytes(result.stdout+result.stderr)
            child=json.loads((output/'result.json').read_text(encoding='utf-8'))
            assert result.returncode==1 and child['status']=='failed' and child['redHits']==0 and child['regionPixels']==2432 and child['minimumFraction']==.75,child
            assert child['validationErrors']==child['liveBuffers']==child['liveImages']==0 and child['composition']=='none',child
            assert not any((output/'2.bin').read_bytes()),child
            report['cases'].append(dict(mode=mode,exitCode=result.returncode,result=child,command=command,partialBlend=.75))
            baseline=(run/'history-response-tracked/2.bin').read_bytes();ratios=[]
            for mode in ('history-response-cutout','history-response-partial'):
                values=(run/mode/'2.bin').read_bytes();assert len(values)==len(baseline)==320*240
                maximum=max(abs(value-round(reference*.25)) for value,reference in zip(values,baseline));assert maximum<=1,(mode,maximum)
                assert all((run/mode/name).read_bytes()==(run/'history-response-tracked'/name).read_bytes() for name in ('0.bin','1.bin'))
                ratios.append(dict(mode=mode,expectedAcceptance=.25,maximumUnormByteError=maximum,colorExact=True,footprintExact=True))
            assert all((run/'history-response-assumed'/name).read_bytes()==(run/'history-response-tracked'/name).read_bytes() for name in ('0.bin','1.bin','2.bin'))
            for mode in ('history-response-background','history-response-missing','history-response-reactive','history-response-nonfinite','history-response-depth','history-response-zero-depth','history-response-outside'):
                assert not any((run/mode/'2.bin').read_bytes()),mode
            report['motionResponse']=dict(ratios=ratios,defaultPolicyControlExact=True,invalidConfidenceAllZero=True,partialBlendRejected=True)
        else:
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
