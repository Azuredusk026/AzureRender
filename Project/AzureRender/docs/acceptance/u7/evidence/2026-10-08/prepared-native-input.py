"""Validate native Windows messages through GLFW and the production editor route."""
import argparse
import ctypes
import json
import os
from pathlib import Path
import secrets
import shutil
import subprocess
import time
from create_playable_project import create
from run_playable_long_run import NativeWindow
from test_validation_protocol import exchange


def run(executable, output, workflow):
    output.mkdir(parents=True)
    project=output/'project'
    if workflow=='exploration': create(project)
    else: shutil.copytree(Path(__file__).resolve().parents[1]/'assets_public/scene_inspector',project,ignore=shutil.ignore_patterns('.azure'))
    token=secrets.token_hex(32); endpoint_path=output/'endpoint.json'
    env=dict(os.environ,AZURE_NATIVE_TOKEN=token,AZURERENDER_EDITOR_CONFIG=str(output/'config'))
    logs=[(output/name).open('w',encoding='utf-8') for name in ('stdout.log','stderr.log')]
    process=subprocess.Popen([str(executable),'--editor-project',str(project/'project.azureproject'),'--editor-close-policy','discard',
        '--width','1280','--height','720','--fixed-frame-step','--validation-token-env','AZURE_NATIVE_TOKEN',
        '--validation-endpoint',str(endpoint_path),'--runtime-report',str(output/'runtime.json')],env=env,stdout=logs[0],stderr=logs[1])
    evidence=[]
    try:
        deadline=time.monotonic()+45
        while not endpoint_path.exists() and time.monotonic()<deadline:
            assert process.poll() is None;time.sleep(.02)
        endpoint=json.loads(endpoint_path.read_text(encoding='utf-8'))
        def call(op,**parameters):
            response=exchange(endpoint,token,dict(op=op,**parameters));assert response['passed'],response
            evidence.append(dict(op=op,parameters=parameters,response=response));return response.get('value')
        def wait(frames=5):call('wait-frames',frames=frames,timeoutMs=15000)
        def state():return json.loads(call('query',name='editor.workspace'))
        window=NativeWindow(process.pid);assert window.find();window.hide()
        def post(message,wparam=0,lparam=0):
            assert window.user.PostMessageW(window.handle,message,wparam,lparam)
            evidence.append(dict(nativeMessage=message,wparam=wparam,lparam=lparam));wait()
        def key(value,down):
            code=ord(value);scan=window.user.MapVirtualKeyW(code,0)
            post(0x100 if down else 0x101,code,1|(scan<<16)|(0 if down else (1<<30)|(1<<31)))
        def mouse(message,x,y,buttons=0):post(message,buttons,(int(x)&0xffff)|((int(y)&0xffff)<<16))
        wait(20); baseline=state(); saved=call('query',name='document.contentHash')
        image=baseline['image'];x=image['x']+image['width']/2;y=image['y']+image['height']/2
        post(0x7);mouse(0x200,x,y);mouse(0x201,x,y,1);mouse(0x202,x,y)
        for value,expected in (('E',1),('R',2),('W',0),('Q',3)):
            key(value,True);key(value,False);assert state()['gizmoMode']==expected,(value,state()['gizmoMode'])
        mouse(0x200,x,y);mouse(0x204,x,y,2);assert state()['capture']['navigationButton']==1
        before=state()['camera'];mouse(0x200,x+24,y-18,2);turned=state()
        assert turned['camera']!=before,'Native pointer motion must rotate the editor view'
        mouse(0x205,x+image['width'],y);assert state()['capture']['navigationButton']==-1
        mouse(0x200,x,y);mouse(0x204,x,y,2);post(0x8)
        assert state()['capture']['navigationButton']==-1,'Native focus loss must release capture'
        assert call('query',name='document.contentHash')==saved,'Native view navigation must preserve authored content'
        assert window.resize(1920,1080);wait(12);assert window.resize(1280,720);wait(12)
        post(0x7);window.close();assert process.wait(timeout=30)==0
        summary=dict(workflow=workflow,passed=True,inputSource='Windows PostMessage -> GLFW callbacks',physicalKeyboardMouse='NotExecuted',
            tools=True,rightLook=True,outsideRelease=True,focusRelease=True,resize=True,contentPreserved=True)
        (output/'summary.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
        return summary
    finally:
        if process.poll() is None:process.terminate();process.wait(timeout=5)
        for log in logs:log.close()
        (output/'events.json').write_text(json.dumps(evidence,indent=2),encoding='utf-8')


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--executable',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();root=args.output.resolve()/('run-'+str(time.time_ns()))
    results=[run(args.executable.resolve(),root/workflow,workflow) for workflow in ('exploration','inspector')]
    (root/'summary.json').write_text(json.dumps(results,indent=2),encoding='utf-8');print(json.dumps(results))
