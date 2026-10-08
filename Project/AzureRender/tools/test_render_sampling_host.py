"""Exercise sampling changes, resize, save/reopen and asset reload in the real editor."""
import argparse,json,os,secrets,shutil,subprocess,time,ctypes
from pathlib import Path
from ctypes import wintypes
from test_validation_protocol import exchange
from run_playable_long_run import NativeWindow

def run(executable,root):
    root.mkdir(parents=True,exist_ok=True);project=root/'project'
    shutil.copytree(Path(__file__).resolve().parents[1]/'assets_public/scene_inspector',project,ignore=shutil.ignore_patterns('.azure'))
    token=secrets.token_hex(32);env=dict(os.environ,AZURE_SAMPLING_TOKEN=token,AZURERENDER_EDITOR_CONFIG=str(root/'config'),VK_LAYER_VALIDATE_SYNC='1')
    endpoint_file=root/'endpoint.json';logs=[(root/name).open('w',encoding='utf-8') for name in ['stdout.log','stderr.log']]
    startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    command=[str(executable),'--editor-project',str(project/'project.azureproject'),'--editor-close-policy','save','--width','1280','--height','720',
             '--fixed-frame-step','--validation-token-env','AZURE_SAMPLING_TOKEN','--validation-endpoint',str(endpoint_file)]
    process=subprocess.Popen(command,env=env,stdout=logs[0],stderr=logs[1],startupinfo=startup);evidence=[]
    try:
        deadline=time.monotonic()+45
        while not endpoint_file.exists() and time.monotonic()<deadline:
            assert process.poll() is None;time.sleep(.02)
        endpoint=json.loads(endpoint_file.read_text(encoding='utf-8'))
        def call(op,passed=True,**parameters):
            response=exchange(endpoint,token,dict(op=op,**parameters));evidence.append(response)
            assert response['passed']==passed,response;return response.get('value')
        def edit(command,passed=True,**parameters):
            base=json.loads(call('query',name='document.version'))
            return call('edit',passed=passed,requestId='sampling-'+str(len(evidence)),commandId=command,parameters=parameters,baseVersion=base)
        def frames():call('wait-frames',frames=12,timeoutMs=30000)
        def inspect(quality):
            frames();value=json.loads(call('query',name='render.sampling'));scale=2 if quality==2 else 1
            assert value['quality']==quality and value['sceneExtent']==[v*scale for v in value['outputExtent']],value
            return value
        frames();initial=call('query',name='document.version')
        for quality in [0,2,1,2,0,2]:
            edit('settings.set',name='render.antiAliasing',value=quality);inspect(quality)
        edit('settings.set',passed=False,name='render.antiAliasing',value=3)
        assert call('query',name='document.version')==initial
        before=inspect(2);window=NativeWindow(process.pid);assert window.find()
        window.user.SetWindowPos.argtypes=[wintypes.HWND,wintypes.HWND,ctypes.c_int,ctypes.c_int,ctypes.c_int,ctypes.c_int,wintypes.UINT]
        assert window.user.SetWindowPos(window.handle,None,0,0,1440,900,0x14)
        after=inspect(2);assert before['outputExtent']!=after['outputExtent']
        edit('assets.reload');inspect(2)
        edit('settings.reset',name='render.antiAliasing')
        edit('render.settings',values={'antiAliasing':2,'outlinePixels':.8});inspect(2)
        edit('document.save');frames();edit('document.reload');inspect(2)
        window.user.PostMessageW.argtypes=[wintypes.HWND,wintypes.UINT,wintypes.WPARAM,wintypes.LPARAM]
        assert window.user.PostMessageW(window.handle,0x10,0,0)
        process.wait(timeout=60)
    finally:
        if process.poll() is None:process.terminate();process.wait(timeout=5)
        for log in logs:log.close()
        (root/'events.json').write_text(json.dumps(evidence,indent=2),encoding='utf-8')
    combined=''.join((root/name).read_text(encoding='utf-8') for name in ['stdout.log','stderr.log'])
    assert process.returncode==0 and 'VUID-' not in combined and 'Validation Error' not in combined,combined[-4000:]
    assert 'Allocator after unload: buffers=0 images=0' in combined
    reopen=subprocess.run([str(executable),'--editor-project',str(project/'project.azureproject'),'--editor-close-policy','save','--smoke-frames','20'],env=env,startupinfo=startup,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=120)
    (root/'reopen.log').write_text(reopen.stdout+reopen.stderr,encoding='utf-8')
    assert reopen.returncode==0 and 'VUID-' not in reopen.stderr and 'Allocator after unload: buffers=0 images=0' in reopen.stdout
    saved=json.loads((project/'assets/inspection.azurelevel').read_text(encoding='utf-8'))
    assert saved['renderSettings']['antiAliasing']==2,'Authored quality must survive save and reopen'
    report={'qualitySwitches':6,'supersamplingResize':True,'assetReload':True,'saveReopen':True,'validationErrors':0,'resourcesAfterUnload':0}
    (root/'summary.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(report))
if __name__=='__main__':
    p=argparse.ArgumentParser(__doc__);p.add_argument('--executable',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    args=p.parse_args();root=args.output.resolve();root.mkdir(parents=True,exist_ok=True)
    output=root/('run-'+str(time.time_ns()));run(args.executable.resolve(),output)
    shutil.copyfile(output/'summary.json',root/'summary.json')
