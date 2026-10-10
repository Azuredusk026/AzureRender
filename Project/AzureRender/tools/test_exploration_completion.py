"""Complete the authored exploration using ordinary Player input."""
import argparse,json,math,os,subprocess,uuid
from pathlib import Path

def run(executable,project,output,capture=False):
    root=output/('run-'+uuid.uuid4().hex[:8]);root.mkdir(parents=True)
    design=json.loads((project.parent/'EXPLORATION-DESIGN.json').read_text(encoding='utf-8'))
    fps=4;speed=design['walkSpeed'];frame=8;events=[]
    def key(f,key,down):events.append(dict(frame=f,action='key',key=key,down=down))
    def press(f,k):key(f,k,True);key(f+1,k,False)
    press(4,69)
    stops=set(design['interactionStops']);jumped=False
    for index,(a,b) in enumerate(zip(design['walkRoute'],design['walkRoute'][1:]),1):
        delta=[b[i]-a[i] for i in range(2)]
        assert sum(abs(d)>1e-5 for d in delta)==1,'Route replay uses measured straight legs'
        axis=0 if delta[0] else 1;distance=abs(delta[axis]);direction=(68 if delta[0]>0 else 65) if axis==0 else (83 if delta[1]>0 else 87)
        duration=round(distance/speed*fps);assert duration>0
        key(frame,direction,True);key(frame+duration,direction,False)
        if index==7:press(frame+8,32);jumped=True
        frame+=duration+4
        if index in stops:press(frame,69);frame+=4
    completion_end=frame+12;press(completion_end,82);press(completion_end+8,69)
    # Sprint after restart provides a separate actual input-speed observation.
    start=completion_end+12;key(start,340,True);key(start,87,True);key(start+8,87,False);key(start+8,340,False)
    press(start+16,82);count=start+28
    events.sort(key=lambda e:e['frame']);inputs=root/'input.json';inputs.write_text(json.dumps({'schemaVersion':1,'actions':events},indent=2)+'\n',encoding='utf-8')
    report=root/'runtime.json';command=[str(executable.resolve()),'--project',str(project.resolve()),'--width','1280','--height','720','--game-actions',str(inputs.resolve()),'--runtime-report',str(report.resolve()),'--capture-fps',str(fps)]
    command+=['--capture-dir',str((root/'capture').resolve()),'--capture-frames',str(count)] if capture else ['--fixed-frame-step','--smoke-frames',str(count)]
    startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    result=subprocess.run(command,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=600,startupinfo=startup)
    (root/'stdout.log').write_text(result.stdout,encoding='utf-8');(root/'stderr.log').write_text(result.stderr,encoding='utf-8')
    assert result.returncode==0,result.stdout[-4000:]+result.stderr[-4000:]
    assert 'Allocator after unload: buffers=0 images=0' in result.stdout
    assert 'VUID-' not in result.stderr and 'Validation Error' not in result.stderr
    data=json.loads(report.read_text(encoding='utf-8'));assert not data['scriptErrors'] and not data['presentationErrors']
    route=data['routeFrames'];completed=[f for f in route if any(t['completed'] for t in f['tasks'])];assert completed,'Ordinary input must complete the authored route'
    finish=completed[0];assert finish['tasks'][0]['collected']==3 and finish['doorOpen']
    assert any(f['checkpointActivated'] for f in route) and any(f['cameraObstructed'] for f in completed)
    assert data['replayedActions']==len(events)
    first=route[0]['levelRevision'];assert route[-1]['levelRevision']==first+2
    assert not route[-1]['tasks'][0]['started'] and route[-1]['collectibles']==3
    player=lambda f:next(c for c in f['characters'] if c['node']=='hero:body')
    positions=[player(f)['position'] for f in route if f['frame']<=finish['frame']]
    assert all(abs(p[0])<12 and p[1]>-.2 for p in positions),'Route has no floor fallthrough or out-of-bounds motion'
    assert jumped and max(p[1] for p in positions)>.8,'Input jump leaves the floor'
    sprint=[player(f)['position'] for f in route if start+1<=f['frame']<=start+8]
    sprint_speed=math.dist(sprint[0],sprint[-1])/((len(sprint)-1)/fps);assert sprint_speed>speed*2
    summary=dict(passed=True,completionSeconds=finish['frame']/fps,pathMeters=sum(math.dist(a,b) for a,b in zip(positions,positions[1:])),sprintSpeed=sprint_speed,jumpHeight=max(p[1] for p in positions),checkpoints=True,cameraObstruction=True,restarts=2,released=True,actions=len(events))
    (root/'summary.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8');print(json.dumps(summary));return root

if __name__=='__main__':
    p=argparse.ArgumentParser(__doc__);p.add_argument('--executable',type=Path,required=True);p.add_argument('--project',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--capture',action='store_true')
    a=p.parse_args();run(a.executable,a.project,a.output,a.capture)
