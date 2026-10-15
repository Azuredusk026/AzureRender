"""Exercise progressive rendering, edits and renderer replacement on the real GPU."""
import argparse
import json
import os
from pathlib import Path
import subprocess
from create_pathtracing_project import create


def run(executable, root):
    root.mkdir(parents=True, exist_ok=True)
    results = {}
    for name, sky in [('room',2),('outdoor',0)]:
        folder=root/name
        project=create(folder/'project')
        level=project.parent/'assets/room.azurelevel'
        scene=json.loads(level.read_text(encoding='utf-8'))
        scene['renderSettings'].update(skyMode=sky,pathTraceSamples=12)
        if name=='outdoor':
            scene['nodes']=[n for n in scene['nodes'] if n['id'] not in ('back','left','right')]
        level.write_text(json.dumps(scene),encoding='utf-8')
        actions=[]
        if name=='outdoor':
            for frame,renderer in [(20,'character'),(35,'pathtracing')]:
                actions.append(dict(frame=frame,command='edit',operation='render.settings',parameters={'values':{'renderer':renderer}}))
            actions.append(dict(frame=50,command='edit',operation='render.settings',parameters={'values':{'skyRotation':90}}))
        task=folder/'actions.json';task.write_text(json.dumps(actions),encoding='utf-8')
        report=folder/'runtime.json'
        command=[str(executable),'--editor-project',str(project),'--editor-close-policy','discard','--width','960','--height','640','--fixed-frame-step','--smoke-frames','76','--editor-actions',str(task),'--runtime-report',str(report)]
        env=dict(os.environ,AZURERENDER_EDITOR_CONFIG=str(folder/'config'))
        result=subprocess.run(command,cwd=folder,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,encoding='utf-8',errors='replace',timeout=120)
        (folder/'run.log').write_text(result.stdout,encoding='utf-8')
        assert result.returncode==0,(name,result.returncode,result.stdout[-3000:])
        assert '[validation error]' not in result.stdout and 'VUID-' not in result.stdout,result.stdout
        assert 'Allocator after unload: buffers=0 images=0' in result.stdout
        data=json.loads(report.read_text(encoding='utf-8'))
        assert data['renderer']=='pathtracing',data
        assert data['renderProgress']=={'samples':12,'target':12},data['renderProgress']
        assert all(a['passed'] for a in data.get('editorActions',[])),data
        assert len(data.get('editorActions',[]))==len(actions),data
        results[name]={'samples':data['renderProgress'],'actions':len(actions),'validationEnabled':'Validation layer: enabled' in result.stdout,'reportedValidationErrors':0,'released':True}
    (root/'summary.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
    print(json.dumps(results))

if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();run(args.executable.resolve(),args.output.resolve())
