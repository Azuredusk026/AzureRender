"""Exercise the exploration quest and restart with normal Player input."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile
from create_playable_project import create

def run(executable, root, character=None, capture=False, timeout=600, frames=940, project=None, env=None, cwd=None):
    root.mkdir(parents=True,exist_ok=True)
    if project is None:
        create(root/'game',character)
        project=root/'game/project.azureproject'
    events=[]
    def key(frame,value,down): events.append({'frame':frame,'action':'key','key':value,'down':down})
    def press(frame,value): key(frame,value,True);key(frame+1,value,False)
    def move(start,end,value): key(start,value,True);key(end,value,False)
    press(4,69)
    for a,b,k in ((8,108,87),(112,140,65),(148,176,68),(180,340,87),(344,372,68),
        (380,408,65),(412,572,87),(576,604,65),(612,640,68),(644,800,87),(806,898,87)):
        move(a,b,k)
    for frame in (144,376,608,802): press(frame,69)
    press(910,82);press(922,69);press(926,82)
    events.sort(key=lambda e:e['frame'])
    events=[e for e in events if e['frame']<frames]
    inputs=root/'input.json';inputs.write_text(json.dumps({'schemaVersion':1,'actions':events},indent=2),encoding='utf-8')
    report=root/'runtime.json'
    command=[str(executable.resolve()),'--project',str(project.resolve()),'--width','960','--height','540',
        '--runtime-report',str(report.resolve()),'--game-actions',str(inputs.resolve()),'--capture-fps','4']
    command+=['--capture-dir',str((root/'capture').resolve()),'--capture-frames',str(frames)] if capture else ['--fixed-frame-step','--smoke-frames',str(frames)]
    info=subprocess.STARTUPINFO();info.dwFlags|=subprocess.STARTF_USESHOWWINDOW;info.wShowWindow=0
    result=subprocess.run(command,capture_output=True,encoding='utf-8',errors='replace',timeout=timeout,startupinfo=info,env=env,cwd=cwd)
    (root/'stdout.log').write_text(result.stdout,encoding='utf-8');(root/'stderr.log').write_text(result.stderr,encoding='utf-8')
    assert result.returncode==0,result.stdout+result.stderr
    assert 'VUID-' not in result.stderr and 'Validation Error' not in result.stderr,result.stderr
    assert 'Allocator after unload: buffers=0 images=0' in result.stdout
    data=json.loads(report.read_text(encoding='utf-8'))
    assert not data['scriptErrors'] and not data['presentationErrors'],data
    route=data['routeFrames'];assert all('tasks' in frame for frame in route),'Task state must be observable in route evidence'
    if frames<900: return data
    completed=[f for f in route if any(t['completed'] for t in f['tasks'])]
    assert completed,'Quest must reach the goal after collecting and opening the gate'
    finished=completed[0];task=finished['tasks'][0]
    assert task['started'] and task['collected']==3 and task['doorOpened'],task
    assert 180<=finished['frame']/4<=300,finished['frame']
    assert any(f['checkpointActivated'] for f in route),'Checkpoint was not reached'
    assert any(f['cameraObstructed'] for f in route if f['frame']>810),'Interior must exercise camera obstruction'
    final=route[-1];assert final['levelRevision']==route[0]['levelRevision']+2,final
    assert final['tasks'][0]['collected']==0 and not final['tasks'][0]['started'] and not final['tasks'][0]['completed'],final
    assert final['collectibles']==3 and not final['doorOpen'] and not final['checkpointActivated'],final
    assert data['uiDrawCalls']>0 and data['animationFrames']>0,data
    assert data['replayedActions']==len(events)
    summary={'frames':len(route),'actions':len(events),'completionSeconds':finished['frame']/4,
        'fixedSteps':data['fixedSteps'],'restarts':final['levelRevision']-route[0]['levelRevision'],'validationErrors':0,'released':True}
    (root/'summary.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8');print(json.dumps(summary))
    return data

if __name__=='__main__':
    p=argparse.ArgumentParser(__doc__);p.add_argument('--executable',type=Path,required=True);p.add_argument('--character',type=Path)
    p.add_argument('--output',type=Path);p.add_argument('--capture',action='store_true');p.add_argument('--timeout',type=float,default=600);p.add_argument('--frames',type=int,default=940)
    p.add_argument('--project',type=Path,help='Verify an already authored or packaged exploration project')
    a=p.parse_args()
    with tempfile.TemporaryDirectory(prefix='azure_quest_') as temporary:
        run(a.executable,a.output.resolve() if a.output else Path(temporary),a.character,a.capture,a.timeout,a.frames,a.project)
