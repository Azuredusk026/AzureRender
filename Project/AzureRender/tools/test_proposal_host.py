"""Replay optional content assistance through real controls in two projects."""
import argparse
import json
import os
from pathlib import Path
import secrets
import shutil
import subprocess
import sys
import tempfile
import time
from create_playable_project import create
from test_validation_protocol import exchange
from run_playable_long_run import NativeWindow

def close_host(process):
    assert process.poll() is None,'Host exited before all asynchronous checks completed'
    window=NativeWindow(process.pid)
    assert window.find(),'Owned editor window missing at normal shutdown'
    window.close()
    process.wait(timeout=120)

def artifact(command,parameters):
    return dict(schemaVersion=1,operations=[dict(command=command,parameters=parameters)])

def run(executable,output,workflow):
    folder=output/workflow;folder.mkdir(parents=True)
    project=folder/'Project with spaces'
    if workflow=='exploration':create(project)
    else:shutil.copytree(Path(__file__).resolve().parents[1]/'assets_public/scene_inspector',project,ignore=shutil.ignore_patterns('.azure'))
    fixture=folder/'response.json';scene=artifact('node.create',{'id':'ai-candidate'})
    def fixed(**fields):fixture.write_text(json.dumps(dict(responses=[scene],**fields)),encoding='utf-8')
    fixed()
    endpoint_file=folder/'endpoint.json';token=secrets.token_hex(32)
    env=dict(os.environ,AZURE_PROPOSAL_TEST_TOKEN=token,AZURERENDER_EDITOR_CONFIG=str(folder/'config'),AZURERENDER_EDITOR_DPI='1')
    startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    logs=[(folder/name).open('w',encoding='utf-8') for name in ('stdout.log','stderr.log')]
    command=[str(executable),'--editor-close-policy', 'save', '--editor-project',str(project/'project.azureproject'),'--width','1920','--height','1080',
        '--fixed-frame-step','--set','ai.enabled=true','--ai-python',sys.executable,'--ai-fixture',str(fixture),
        '--validation-token-env','AZURE_PROPOSAL_TEST_TOKEN','--validation-endpoint',str(endpoint_file),'--runtime-report',str(folder/'runtime.json')]
    process=subprocess.Popen(command,cwd=folder,env=env,startupinfo=startup,stdout=logs[0],stderr=logs[1])
    evidence=[]
    try:
        deadline=time.monotonic()+45
        while not endpoint_file.exists() and time.monotonic()<deadline:
            assert process.poll() is None,'Editor exited before proposal endpoint';time.sleep(.02)
        endpoint=json.loads(endpoint_file.read_text(encoding='utf-8'))
        def call(op,passed=True,**fields):
            result=exchange(endpoint,token,dict(op=op,**fields));evidence.append(dict(op=op,fields=fields,response=result))
            assert result['passed']==passed,result
            return result.get('value')
        def query(name):return call('query',name=name)
        def edit(command,passed=True,**parameters):
            base=json.loads(query('document.version'))
            return call('edit',passed=passed,requestId='proposal-'+str(len(evidence)),commandId=command,parameters=parameters,baseVersion=base)
        def frames(count=5):call('wait-frames',frames=count)
        def wait(state):call('wait-until',name='ai.state',equals=state)
        def click(target):call('input',event=dict(action='click',target=target))
        def report():return json.loads(query('ai.proposal'))
        def generate(runId,domain='scene'):
            edit('ai.generate',runId=runId,domain=domain,target='document',instruction='Produce the declared structured candidate')
        frames();assert query('ai.available') is True
        initial=query('document.version');count=query('scene.nodeCount')
        click('tool.console');frames();click('ai.instruction')
        call('input',event=dict(action='text',text='Create a scene candidate'))
        frames();click('ai.generate');wait('Ready')
        assert query('document.version')==initial,'Preview changed authored content'
        assert report()['diff'],'Scene preview must expose its difference'
        call('screenshot',label='g9-scene-preview');call('wait-until',name='engine.screenshots',equals=1)
        click('ai.reject');wait('Rejected');assert query('document.version')==initial
        generate('apply');wait('Ready');click('ai.apply');wait('Applied')
        assert query('scene.nodeCount')==count+1
        edit('history.undo');assert query('scene.nodeCount')==count
        assert json.loads(query('document.version'))['contentHash']==json.loads(initial)['contentHash']
        generate('stale');wait('Ready');edit('node.create',id='manual-stale');wait('Stale')
        manual=query('document.version');edit('ai.apply',passed=False)
        assert query('document.version')==manual
        fixed(delayMs=2000);generate('cancel');wait('Generating');edit('ai.cancel');wait('Cancelled');frames(30)
        assert query('document.version')==manual
        fixture.write_text(json.dumps(dict(responses=['{invalid'],runs={'repair-repair-1':scene})),encoding='utf-8')
        generate('repair');wait('Ready');assert report()['repairCount']==1
        edit('ai.reject')
        fixture.write_text(json.dumps(dict(responses=['{invalid'])),encoding='utf-8')
        generate('exhaust');wait('Error');assert report()['repairCount']==1
        assert query('document.version')==manual
        asset=artifact('asset.generate',dict(generator='azure.text',output='assets:/ai-generated.json',license='CC0 test fixture',
            parameters={'source':'{"schemaVersion":1,"tool":"reusable"}'}))
        fixture.write_text(json.dumps(dict(responses=[asset])),encoding='utf-8')
        generated=project/'assets/ai-generated.json'
        generate('asset-preview','asset-parameters');wait('Ready');assert not generated.exists()
        edit('ai.reject');assert not generated.exists()
        generate('asset-apply','asset-parameters');wait('Ready');edit('ai.apply');wait('Applied')
        assert json.loads(generated.read_text(encoding='utf-8'))['tool']=='reusable'
        assert 'generation' in json.loads(Path(str(generated)+'.azmeta').read_text(encoding='utf-8'))
        edit('document.save')
        close_host(process)
    finally:
        if process.poll() is None:process.terminate();process.wait(timeout=5)
        for file in logs:file.close()
        (folder/'control-results.json').write_text(json.dumps(evidence,indent=2)+'\n',encoding='utf-8')
    assert process.returncode==0,(folder/'stderr.log').read_text(encoding='utf-8')
    assert 'VUID-' not in (folder/'stderr.log').read_text(encoding='utf-8')
    assert 'Allocator after unload: buffers=0 images=0' in (folder/'stdout.log').read_text(encoding='utf-8')
    (folder/'control-results.json').write_text(json.dumps(evidence,indent=2)+'\n',encoding='utf-8')
    # A fresh host leaves optional assistance disabled while retaining production editing.
    disabled_endpoint=folder/'disabled-endpoint.json'
    disabled_logs=[(folder/name).open('w',encoding='utf-8') for name in ('disabled-stdout.log','disabled-stderr.log')]
    off=subprocess.Popen([str(executable),'--editor-close-policy', 'save', '--editor-project',str(project/'project.azureproject'),
        '--validation-token-env','AZURE_PROPOSAL_TEST_TOKEN','--validation-endpoint',str(disabled_endpoint),
        '--runtime-report',str(folder/'disabled.json')],cwd=folder,env=env,startupinfo=startup,stdout=disabled_logs[0],stderr=disabled_logs[1])
    disabled_evidence=[]
    try:
        deadline=time.monotonic()+45
        while not disabled_endpoint.exists() and time.monotonic()<deadline:
            assert off.poll() is None,'Disabled host exited before endpoint';time.sleep(.02)
        address=json.loads(disabled_endpoint.read_text(encoding='utf-8'))
        def disabled_call(op,**fields):
            result=exchange(address,token,dict(op=op,**fields));disabled_evidence.append(result)
            assert result['passed'],result
            return result.get('value')
        disabled_call('assert',name='ai.available',equals=False)
        disabled_call('assert',name='ai.state',equals='Disabled')
        for command,parameters in [('node.create',{'id':'manual-without-model'}),('history.undo',{}),
                                   ('preview.play',{}),('preview.stop',{})]:
            base=json.loads(disabled_call('query',name='document.version'))
            disabled_call('edit',requestId='disabled-'+command,commandId=command,parameters=parameters,baseVersion=base)
            if command=='preview.play':disabled_call('wait-until',name='editor.playing',equals=True)
        disabled_call('assert',name='editor.playing',equals=False)
        close_host(off)
    finally:
        if off.poll() is None:off.terminate();off.wait(timeout=5)
        for file in disabled_logs:file.close()
    assert off.returncode==0,(folder/'disabled-stderr.log').read_text(encoding='utf-8')
    assert 'Allocator after unload: buffers=0 images=0' in (folder/'disabled-stdout.log').read_text(encoding='utf-8')
    (folder/'disabled-controls.json').write_text(json.dumps(disabled_evidence,indent=2)+'\n',encoding='utf-8')
    return dict(workflow=workflow,operations=len(evidence),realGenerateRejectApply=True,scenePreview=True,assetPreview=True,
        staleRejected=True,repairOnce=True,repairExhausted=True,cancelled=True,undo=True,disabledHost=True,disabledManualEditing=True,disabledPreview=True,released=True)

def main(executable,output):
    output.mkdir(parents=True)
    result=dict(schemaVersion=1,status='passed',service='real optional process with fixed responses',modelQuality='not evaluated',
        workflows=[run(executable.resolve(),output.resolve(),name) for name in ('exploration','inspector')],physicalKeyboardMouse='not performed')
    (output/'summary.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print(json.dumps(result))

if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--executable',type=Path,required=True);parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    if args.output:main(args.executable,args.output)
    else:
        with tempfile.TemporaryDirectory(prefix='azure proposal host ') as temporary:main(args.executable,Path(temporary)/'evidence')
