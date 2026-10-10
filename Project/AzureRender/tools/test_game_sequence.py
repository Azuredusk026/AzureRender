"""Complete exploration, enter its successor and finish the platform course."""
import argparse,json,subprocess,uuid
from pathlib import Path
from test_exploration_completion import run as exploration_run
def run(executable,planner,exploration,platformer,output):
    baseline=exploration_run(executable,exploration,output/'exploration')
    root=output/('sequence-'+uuid.uuid4().hex[:8]);root.mkdir(parents=True)
    subprocess.run([str(planner.resolve()),str(platformer.resolve()),str(root.resolve())],check=True,timeout=120)
    course=json.loads((root/'input.json').read_text(encoding='utf-8'))['actions'];planned=json.loads((root/'summary.json').read_text(encoding='utf-8'));fps=planned['fps']
    start=json.loads((baseline/'input.json').read_text(encoding='utf-8'))['actions'];cutoff=min(e['frame'] for e in start if e.get('key')==82)
    actions=[dict(e,frame=round(e['frame']*fps/4)) for e in start if e['frame']<cutoff]
    enter=round(cutoff*fps/4);actions += [dict(frame=enter,action='key',key=69,down=True),dict(frame=enter+1,action='key',key=69,down=False)]
    offset=enter+90;actions += [dict(e,frame=e['frame']+offset) for e in course];count=offset+planned['frames']+15
    (root/'input.json').write_text(json.dumps(dict(schemaVersion=1,actions=actions),indent=2)+'\n',encoding='utf-8')
    command=[str(executable.resolve()),'--project',str(exploration.resolve()),'--game-actions',str((root/'input.json').resolve()),'--runtime-report',str((root/'runtime.json').resolve()),'--width','1280','--height','720','--fixed-frame-step','--capture-fps',str(fps),'--smoke-frames',str(count)]
    startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    result=subprocess.run(command,capture_output=True,text=True,encoding='utf-8',errors='replace',startupinfo=startup,timeout=600)
    (root/'stdout.log').write_text(result.stdout,encoding='utf-8');(root/'stderr.log').write_text(result.stderr,encoding='utf-8')
    assert result.returncode==0,result.stdout[-3000:]+result.stderr[-3000:]
    assert 'Allocator after unload: buffers=0 images=0' in result.stdout and 'VUID-' not in result.stderr
    d=json.loads((root/'runtime.json').read_text(encoding='utf-8'));assert not d['scriptErrors'] and not d['presentationErrors']
    finished=[f for f in d['routeFrames'] if any(t['completed'] for t in f['tasks'])];assert finished
    expedition=[f for f in finished if f['tasks'][0]['node']=='hero:body'];course=[f for f in finished if f['tasks'][0]['node']=='hero']
    assert expedition and course,'Both games must complete in the same Player process'
    assert expedition[0]['tasks'][0]['collected']==3 and course[0]['tasks'][0]['collected']==8
    assert course[0]['levelRevision']==expedition[0]['levelRevision']+1
    summary=dict(passed=True,explorationCompleted=True,platformerCompleted=True,successorTransition=True,courseCoins=8,released=True,frames=count)
    (root/'sequence-summary.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8');print(json.dumps(summary));return root
if __name__=='__main__':
    p=argparse.ArgumentParser(__doc__);p.add_argument('--executable',type=Path,required=True);p.add_argument('--planner',type=Path,required=True);p.add_argument('--exploration',type=Path,required=True);p.add_argument('--platformer',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();run(a.executable,a.planner,a.exploration,a.platformer,a.output)
