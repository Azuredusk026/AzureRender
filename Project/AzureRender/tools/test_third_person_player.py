"""Run the actual Player through start, stop, turn, wall, jump and focus input."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile
from create_third_person_project import create

def main():
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--executable',type=Path,required=True)
    parser.add_argument('--character',type=Path);parser.add_argument('--output',type=Path);parser.add_argument('--capture',action='store_true')
    parser.add_argument('--timeout',type=float,default=300,help='Wall-clock limit for recording, in seconds')
    args=parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='azure_3c_route_') as directory:
        root=args.output.resolve() if args.output else Path(directory);root.mkdir(parents=True,exist_ok=True)
        project=root/'game';create(project,args.character)
        events=[]
        def key(frame,value,down):events.append({'frame':frame,'action':'key','key':value,'down':down})
        key(30,87,True);key(170,87,False);key(200,68,True);key(275,68,False)
        key(290,32,True);key(291,32,False);key(350,65,True)
        events.append({'frame':375,'action':'focus','focused':False});events.append({'frame':400,'action':'focus','focused':True})
        events.append({'frame':410,'action':'camera','x':600,'y':100,'scroll':1})
        key(430,87,True);key(490,87,False)
        input_path=root/'input.json';input_path.write_text(json.dumps({'schemaVersion':1,'actions':events},indent=2),encoding='utf-8')
        report=root/'runtime.json';command=[str(args.executable.resolve()),'--project',str(project/'project.azureproject'),
            '--width','960','--height','540','--runtime-report',str(report),'--game-actions',str(input_path),'--capture-fps','60']
        if args.capture:command+=['--capture-dir',str(root/'capture'),'--capture-frames','540']
        else:command+=['--fixed-frame-step','--smoke-frames','540']
        info=subprocess.STARTUPINFO();info.dwFlags|=subprocess.STARTF_USESHOWWINDOW;info.wShowWindow=0
        result=subprocess.run(command,capture_output=True,encoding='utf-8',errors='replace',timeout=args.timeout,startupinfo=info)
        (root/'stdout.log').write_text(result.stdout,encoding='utf-8');(root/'stderr.log').write_text(result.stderr,encoding='utf-8')
        assert result.returncode==0,result.stdout+result.stderr
        assert 'VUID-' not in result.stderr and 'Validation Error' not in result.stderr,result.stderr
        assert 'Allocator after unload: buffers=0 images=0' in result.stdout,result.stdout
        data=json.loads(report.read_text(encoding='utf-8'));assert not data['presentationErrors'] and not data['scriptErrors'],data
        assert data['replayedActions']==len(events),data['replayedActions']
        route=data['routeFrames'];heroes=[next(c for c in r['characters'] if c['node']=='hero') for r in route]
        start=heroes[0]['position'];positions=[h['position'] for h in heroes]
        assert min(p[2] for p in positions)<-3.5,positions[-1]
        assert max(p[0] for p in positions)>1.5,positions[-1]
        assert {'idle','walk'}<={h['state'] for h in heroes}
        assert max(p[1] for p in positions)>start[1]+.8,'Jump did not leave the floor'
        assert all(p[2]>-4.8 and p[0]<3.2 for p in positions),'Character penetrated walls'
        assert any(not r['focused'] for r in route),'Focus loss was not exercised'
        assert heroes[-1]['state']=='idle' and heroes[-1]['grounded'],'Route did not finish at rest'
        summary={'frames':len(route),'actions':len(events),'fixedSteps':data['fixedSteps'],'states':sorted({h['state'] for h in heroes}),
            'finalPosition':positions[-1],'maximumJumpHeight':max(p[1] for p in positions)-start[1],'validationErrors':0}
        (root/'summary.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8');print(json.dumps(summary))

if __name__=='__main__':main()
