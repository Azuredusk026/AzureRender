"""Replay measured keyboard actions through the production Vulkan Player."""
import argparse,json,subprocess,uuid
from pathlib import Path
def run(executable,planner,project,output,capture=False):
    root=output/('run-'+uuid.uuid4().hex[:8]);root.mkdir(parents=True)
    subprocess.run([str(planner.resolve()),str(project.resolve()),str(root.resolve())],check=True,timeout=120)
    planned=json.loads((root/'summary.json').read_text(encoding='utf-8'));inputs=json.loads((root/'input.json').read_text(encoding='utf-8'))
    restart=planned['frames']+15;inputs['actions'] += [dict(frame=restart,action='key',key=82,down=True),dict(frame=restart+1,action='key',key=82,down=False)]
    count=restart+100;(root/'input.json').write_text(json.dumps(inputs,indent=2)+'\n',encoding='utf-8')
    command=[str(executable.resolve()),'--project',str(project.resolve()),'--game-actions',str((root/'input.json').resolve()),'--runtime-report',str((root/'runtime.json').resolve()),'--width','1280','--height','720','--capture-fps',str(planned['fps'])]
    command+=['--capture-dir',str((root/'capture').resolve()),'--capture-frames',str(count)] if capture else ['--fixed-frame-step','--smoke-frames',str(count)]
    startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    result=subprocess.run(command,capture_output=True,text=True,encoding='utf-8',errors='replace',startupinfo=startup,timeout=600)
    (root/'stdout.log').write_text(result.stdout,encoding='utf-8');(root/'stderr.log').write_text(result.stderr,encoding='utf-8')
    assert result.returncode==0,result.stdout[-3000:]+result.stderr[-3000:]
    assert 'Allocator after unload: buffers=0 images=0' in result.stdout and 'VUID-' not in result.stderr
    data=json.loads((root/'runtime.json').read_text(encoding='utf-8'));assert not data['scriptErrors'] and not data['presentationErrors']
    route=data['routeFrames'];hero=lambda f:next(c for c in f['characters'] if c['node']=='hero')
    completed=[f for f in route if f['tasks'][0]['completed']];assert completed,'Player must finish via ordinary input'
    assert completed[0]['tasks'][0]['collected']==8 and completed[0]['doorOpen']
    assert any(hero(f)['position'][1]<-4 for f in route),'Replay must actually fall'
    assert any(f['checkpointActivated'] for f in route)
    assert route[-1]['levelRevision']==route[0]['levelRevision']+1 and route[-1]['collectibles']==10
    assert data['audioStarts']>=20 and data['uiDrawCalls']>0
    for landing in planned['landings']:
        near=[f for f in route if abs(f['frame']-landing['frame'])<=3]
        assert any(hero(f)['grounded'] and abs(hero(f)['position'][2]-landing['position'][2])<1 for f in near),landing
    for node,axis in [('platform-3',0),('platform-5',1)]:
        samples=[next(b for b in f['rigidBodies'] if b['node']==node)['position'][axis] for f in route]
        assert max(samples)-min(samples)>.25,'Moving platform must traverse its path'
    summary=dict(passed=True,mandatoryJumps=7,coins=8,optionalCoins=2,audioStarts=data['audioStarts'],fallRecovery=True,checkpoint=True,movingPlatforms=True,restart=True,released=True,completionSeconds=completed[0]['frame']/planned['fps'],frames=count)
    (root/'gpu-summary.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8');print(json.dumps(summary));return root
if __name__=='__main__':
    p=argparse.ArgumentParser(__doc__);p.add_argument('--executable',type=Path,required=True);p.add_argument('--planner',type=Path,required=True);p.add_argument('--project',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--capture',action='store_true');a=p.parse_args();run(a.executable,a.planner,a.project,a.output,a.capture)
