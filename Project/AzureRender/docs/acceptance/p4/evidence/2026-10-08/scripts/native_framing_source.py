"""Validate native Windows messages through GLFW and the production editor route."""
import argparse
import ctypes
from ctypes import wintypes
from concurrent.futures import ThreadPoolExecutor
import json
import math
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
        window=NativeWindow(process.pid);assert window.find()
        window.user.GetCursorPos.argtypes=[ctypes.POINTER(wintypes.POINT)]
        window.user.ClientToScreen.argtypes=[wintypes.HWND,ctypes.POINTER(wintypes.POINT)]
        window.user.SetCursorPos.argtypes=[ctypes.c_int,ctypes.c_int]
        original_cursor=wintypes.POINT();assert window.user.GetCursorPos(ctypes.byref(original_cursor))
        def post(message,wparam=0,lparam=0,settle=True):
            assert window.user.PostMessageW(window.handle,message,wparam,lparam)
            evidence.append(dict(nativeMessage=message,wparam=wparam,lparam=lparam,monotonicSeconds=time.monotonic()))
            if settle:wait()
        def key(value,down):
            code=0x1B if value=='Escape' else ord(value);scan=window.user.MapVirtualKeyW(code,0)
            post(0x100 if down else 0x101,code,1|(scan<<16)|(0 if down else (1<<30)|(1<<31)))
        def mouse(message,x,y,buttons=0,settle=True):
            point=wintypes.POINT(int(x),int(y));assert window.user.ClientToScreen(window.handle,ctypes.byref(point))
            assert window.user.SetCursorPos(point.x,point.y)
            post(message,buttons,(int(x)&0xffff)|((int(y)&0xffff)<<16),settle=False)
            if settle:wait(2)
        def click_widget(identity,settle=True):
            rect=state()['widgets'][identity];cx=rect[0]+rect[2]/2;cy=rect[1]+rect[3]/2
            mouse(0x200,cx-3,cy-3);mouse(0x200,cx,cy);mouse(0x201,cx,cy,1);mouse(0x202,cx,cy,settle=settle)
        def double_click_widget(identity):
            rect=state()['widgets'][identity];cx=rect[0]+rect[2]/2;cy=rect[1]+rect[3]/2
            mouse(0x200,cx-3,cy-3);mouse(0x200,cx,cy)
            for message,buttons in ((0x201,1),(0x202,0),(0x201,1),(0x202,0)):
                mouse(message,cx,cy,buttons,settle=False)
            wait(8)
        user=window.user
        user.GetClassNameW.argtypes=[wintypes.HWND,wintypes.LPWSTR,ctypes.c_int]
        user.EnumChildWindows.argtypes=[wintypes.HWND,window.callback,wintypes.LPARAM]
        user.GetDlgCtrlID.argtypes=[wintypes.HWND]
        user.SendMessageW.argtypes=[wintypes.HWND,wintypes.UINT,wintypes.WPARAM,wintypes.LPARAM]
        user.SendMessageW.restype=wintypes.LPARAM
        user.IsWindow.argtypes=[wintypes.HWND]
        user.IsWindowVisible.argtypes=[wintypes.HWND]
        def own_dialog():
            found=[]
            def visit(handle,_):
                pid=wintypes.DWORD();user.GetWindowThreadProcessId(handle,ctypes.byref(pid))
                name=ctypes.create_unicode_buffer(256);user.GetClassNameW(handle,name,256)
                if pid.value==process.pid and name.value=='#32770' and user.IsWindowVisible(handle):found.append(handle)
                return True
            user.EnumWindows(window.callback(visit),0)
            return found[0] if found else None
        def choose(identity,path=None,via_browse=False):
            pending=None;pool=None
            if via_browse:
                click_widget('assets.import-create');click_widget(identity,settle=False)
            else:
                pool=ThreadPoolExecutor(max_workers=1)
                purpose=identity.split('.')[0]
                pending=pool.submit(call,'edit',requestId='path-'+str(len(evidence)),commandId='path.choose',
                    parameters=dict(purpose=purpose,directory=purpose=='project',extensions=['.gltf','.glb'] if purpose=='import' else []),
                    baseVersion=json.loads(call('query',name='document.version')),timeoutMs=30000)
            deadline=time.monotonic()+15;dialog=None
            while time.monotonic()<deadline and not dialog:
                dialog=own_dialog();time.sleep(.02)
            assert dialog,'Browse must open a dialog owned by the editor'
            controls=[]
            def visit(handle,_):
                name=ctypes.create_unicode_buffer(256);user.GetClassNameW(handle,name,256)
                controls.append(dict(handle=handle,kind=name.value,id=user.GetDlgCtrlID(handle)));return True
            user.EnumChildWindows(dialog,window.callback(visit),0)
            evidence.append(dict(nativeDialog=identity,controls=controls,selected=str(path) if path else None))
            if path:
                field_id=1152 if identity=='project.browse' else 1148
                field=next(row['handle'] for row in controls if row['kind']=='Edit' and row['id']==field_id)
                value=ctypes.create_unicode_buffer(str(path))
                user.SendMessageW(field,0x0C,0,ctypes.cast(value,ctypes.c_void_p).value)
            button=next(row['handle'] for row in controls if row['kind']=='Button' and row['id']==(1 if path else 2))
            assert user.PostMessageW(button,0xF5,0,0)
            while user.IsWindow(dialog) and time.monotonic()<deadline:time.sleep(.02)
            assert not user.IsWindow(dialog),'Path selection must finish'
            if pending:
                result=pending.result(timeout=15)['value'];pool.shutdown()
                assert result['cancelled']==(path is None),result
                if path:assert Path(result['path'])==path
            wait(10)
        wait(20); baseline=state(); saved=call('query',name='document.contentHash')
        image=baseline['image'];x=image['x']+image['width']/2;y=image['y']+image['height']/2
        post(0x7);mouse(0x200,x,y);mouse(0x201,x,y,1);mouse(0x202,x,y)
        for value,expected in (('E',1),('R',2),('W',0),('Q',3)):
            key(value,True);key(value,False);assert state()['gizmoMode']==expected,(value,state()['gizmoMode'])
        other='hero:body' if workflow=='exploration' else 'asset-b'
        click_widget('outliner.search')
        for char in other:post(0x102,ord(char),settle=False)
        wait(10)
        before_click=state()['camera'];click_widget('node.'+other)
        assert state()['camera']==before_click,'Outliner single click must only select'
        wait(80);double_click_widget('node.'+other)
        assert state()['camera']!=before_click,'Outliner double click must frame the object'
        label='native-double-'+workflow+'-'+str(time.time_ns())
        framed=state()
        call('screenshot',label=label);wait(12)
        capture=executable.parent/'captures'/(label+'.png');assert capture.is_file(),capture
        shutil.copyfile(capture,output/'double-frame.png')
        assert call('query',name='document.contentHash')==saved
        window.close();assert process.wait(timeout=30)==0
        summary=dict(workflow=workflow,passed=True,singleOnlySelects=True,doubleFrames=True,camera=framed['camera'],image=framed['image'],inputSource='Windows PostMessage + SetCursorPos -> GLFW callbacks',physicalKeyboardMouse='NotExecuted')
        (output/'summary.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
        return summary
    finally:
        if 'original_cursor' in locals():window.user.SetCursorPos(original_cursor.x,original_cursor.y)
        if process.poll() is None:process.terminate();process.wait(timeout=5)
        for log in logs:log.close()
        (output/'events.json').write_text(json.dumps(evidence,indent=2),encoding='utf-8')


