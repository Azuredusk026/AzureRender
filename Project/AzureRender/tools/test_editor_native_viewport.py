"""Validate native messages and relative SendInput through GLFW capture.

Retains the native-input fixture assertions; captured mouse movement uses
relative operating-system input because GLFW hides and recenters the cursor.
Original fixture SHA256: 85bda43ae9bf63eece31c43fe5b52509de1b2c7f05794abfdb16ff6da844262a
"""
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
        '--width','1280','--height','720','--validation-token-env','AZURE_NATIVE_TOKEN',
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
        class MouseInput(ctypes.Structure):
            _fields_=[('dx',wintypes.LONG),('dy',wintypes.LONG),('mouseData',wintypes.DWORD),('flags',wintypes.DWORD),('time',wintypes.DWORD),('extra',ctypes.c_size_t)]
        class InputUnion(ctypes.Union):
            _fields_=[('mouse',MouseInput)]
        class Input(ctypes.Structure):
            _fields_=[('type',wintypes.DWORD),('data',InputUnion)]
        window.user.SendInput.argtypes=[wintypes.UINT,ctypes.POINTER(Input),ctypes.c_int]
        window.user.GetForegroundWindow.restype=wintypes.HWND
        logical_mouse=[0,0]
        def activate_window():
            caller=ctypes.WinDLL('kernel32').GetCurrentThreadId()
            foreground=window.user.GetForegroundWindow()
            thread=window.user.GetWindowThreadProcessId(foreground,None)
            attached=thread!=caller and bool(window.user.AttachThreadInput(caller,thread,True))
            try:
                window.user.BringWindowToTop(window.handle)
                assert window.user.SetForegroundWindow(window.handle) or window.user.GetForegroundWindow()==window.handle
            finally:
                if attached:window.user.AttachThreadInput(caller,thread,False)
        def mouse(message,x,y,buttons=0,settle=True):
            captured=state()['navigation']['cursorCaptured']
            if captured and message==0x200:
                activate_window()
                relative=[int(x)-logical_mouse[0],int(y)-logical_mouse[1]]
                event=Input(0,InputUnion(MouseInput(relative[0],relative[1],0,1,0,0)))
                assert window.user.SendInput(1,ctypes.byref(event),ctypes.sizeof(Input))==1
                evidence.append(dict(nativeRelativeMouse=relative,inputSource='Win32 SendInput'))
            else:
                if not captured:
                    point=wintypes.POINT(int(x),int(y));assert window.user.ClientToScreen(window.handle,ctypes.byref(point))
                    assert window.user.SetCursorPos(point.x,point.y)
                post(message,buttons,(int(x)&0xffff)|((int(y)&0xffff)<<16),settle=False)
            logical_mouse[:]=[int(x),int(y)]
            if settle:wait(2)
        def click_widget(identity,settle=True):
            rect=state()['widgets'][identity];cx=rect[0]+rect[2]/2;cy=rect[1]+rect[3]/2
            mouse(0x200,cx-3,cy-3);mouse(0x200,cx,cy);mouse(0x201,cx,cy,1);mouse(0x202,cx,cy,settle=settle)
        def native_click_widget(identity,settle=True):
            activate_window()
            rect=state()['widgets'][identity];point=wintypes.POINT(int(rect[0]+rect[2]/2),int(rect[1]+rect[3]/2))
            assert window.user.ClientToScreen(window.handle,ctypes.byref(point))
            # Refresh enter/leave state after capture release and window resizing.
            assert window.user.SetCursorPos(point.x-3,point.y-3);wait(2)
            assert window.user.SetCursorPos(point.x,point.y);wait(5)
            actual=wintypes.POINT();assert window.user.GetCursorPos(ctypes.byref(actual))
            evidence.append(dict(nativeClickTarget=identity,expectedScreen=[point.x,point.y],
                                 actualScreen=[actual.x,actual.y],foreground=window.user.GetForegroundWindow()==window.handle))
            for flag in (2,4):
                event=Input(0,InputUnion(MouseInput(0,0,0,flag,0,0)))
                assert window.user.SendInput(1,ctypes.byref(event),ctypes.sizeof(Input))==1
                evidence.append(dict(nativeMouseButton=flag,target=identity,inputSource='Win32 SendInput'))
                if flag==2:wait(3)
            if settle:wait(5)
        def double_click_widget(identity):
            activate_window()
            rect=state()['widgets'][identity];cx=rect[0]+rect[2]/2;cy=rect[1]+rect[3]/2
            mouse(0x200,cx-3,cy-3);mouse(0x200,cx,cy)
            for flag in (2,4,2,4):
                event=Input(0,InputUnion(MouseInput(0,0,0,flag,0,0)))
                assert window.user.SendInput(1,ctypes.byref(event),ctypes.sizeof(Input))==1
                evidence.append(dict(nativeMouseButton=flag,target=identity,inputSource='Win32 SendInput double click'))
                wait(2)
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
                activate_window();wait(10)
                native_click_widget('assets.import-create')
                if identity not in state()['widgets']:
                    call('screenshot',label='native-import-missing-'+workflow);wait(5)
                    raise AssertionError('Native click did not open the import popup; inspect cursor and foreground evidence')
                native_click_widget(identity,settle=False)
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
        raw_mouse=workflow=='exploration'
        version=json.loads(call('query',name='document.version'))
        call('edit',requestId='u12-native-mouse-mode',commandId='settings.set',baseVersion=version,
             parameters={'name':'editor.camera.rawMouse','value':raw_mouse})
        activate_window()
        wait(20); baseline=state(); saved=call('query',name='document.contentHash')
        image=baseline['image'];x=image['x']+image['width']/2;y=image['y']+image['height']/2
        # A distinct second move refreshes GLFW/ImGui after queued enter/leave events.
        # Focus away from the selected object's transform handles.
        focus_x=image['x']+20;focus_y=image['y']+20
        post(0x7);mouse(0x200,focus_x-3,focus_y-3);mouse(0x200,focus_x,focus_y);mouse(0x201,focus_x,focus_y,1);mouse(0x202,focus_x,focus_y)
        for value,expected in (('E',1),('R',2),('W',0),('Q',3)):
            key(value,True);key(value,False);assert state()['gizmoMode']==expected,(value,state()['gizmoMode'])
        other='ground' if workflow=='exploration' else 'asset-b'
        before_click=state()['camera'];click_widget('node.'+other)
        assert state()['camera']==before_click,'Outliner single click must only select'
        # Native click deadlines use wall time, independently of rendering speed.
        time.sleep(.35);double_click_widget('node.'+other)
        assert state()['camera']!=before_click,'Outliner double click must frame the object'
        mouse(0x200,x,y);mouse(0x204,x,y,2);assert state()['capture']['navigationButton']==1
        assert state()['navigation']['nativeCursorMode']=='disabled' and state()['navigation']['rawMouse']==raw_mouse
        before=state()['camera'];mouse(0x200,x+24,y-18,2);turned=state()
        assert turned['camera']['position']==before['position'],'Right look must preserve camera position'
        def normalized(v):
            size=math.sqrt(sum(a*a for a in v));return [a/size for a in v]
        forward=normalized([b-a for a,b in zip(before['position'],before['target'])])
        right=normalized([-forward[2],0,forward[0]])
        up=[right[1]*forward[2]-right[2]*forward[1],right[2]*forward[0]-right[0]*forward[2],right[0]*forward[1]-right[1]*forward[0]]
        direction=normalized([b-a for a,b in zip(turned['camera']['position'],turned['camera']['target'])])
        assert sum(a*b for a,b in zip(direction,right))>0 and sum(a*b for a,b in zip(direction,up))>0
        for value in ('W','E','Q'):
            start=state()['camera']['position'];key(value,True)
            assert state()['camera']['position']!=start,'Native navigation key must move: '+value
            assert state()['gizmoMode']==3,'Navigation keys must preserve the selected tool'
            key(value,False);stopped=state()['camera'];wait(8)
            assert state()['camera']==stopped,'Native key release must stop flight'
        mouse(0x205,x+image['width'],y);assert state()['capture']['navigationButton']==-1
        assert state()['navigation']['nativeCursorMode']=='normal' and not state()['navigation']['rawMouse']
        key('F',True);key('F',False)
        assert state()['camera']!=turned['camera'],'F must frame the current selection'
        mouse(0x200,x,y);mouse(0x204,x,y,2);key('W',True);post(0x8)
        assert state()['capture']['navigationButton']==-1,'Native focus loss must release capture'
        stopped=state()['camera'];wait(8);assert state()['camera']==stopped,'Native focus loss must stop flight'
        key('W',False)
        assert call('query',name='document.contentHash')==saved,'Native view navigation must preserve authored content'
        post(0x7)
        for width,height in ((1920,1080),(1280,720)):
            immediate=window.resize(width,height);wait(12)
            actual=window.extent();evidence.append(dict(resizeRequested=[width,height],immediate=immediate,actual=actual))
            assert actual==[width,height],actual
        click_widget('tab.assets')
        history_path=output/'config/path-history.json'
        choose('import.browse',via_browse=True)
        assert not history_path.exists(),'Cancel must preserve path history'
        source=output/'原生选择/cube.gltf';source.parent.mkdir()
        shutil.copyfile(Path(__file__).resolve().parents[1]/'assets_public/scene_inspector/assets/cube.gltf',source)
        choose('import.browse',source)
        remembered=json.loads(history_path.read_text(encoding='utf-8'))
        assert Path(remembered['directories']['import'][0])==source.parent
        choose('import.browse')
        assert json.loads(history_path.read_text(encoding='utf-8'))==remembered
        choose('project.browse',source.parent)
        assert Path(json.loads(history_path.read_text(encoding='utf-8'))['directories']['project'][0])==source.parent
        choose('project.browse')
        assert call('query',name='document.contentHash')==saved
        post(0x7);window.close();assert process.wait(timeout=30)==0
        summary=dict(workflow=workflow,passed=True,inputSource='Windows PostMessage + SetCursorPos + relative and dialog-click SendInput -> GLFW callbacks',physicalKeyboardMouse='NotExecuted',
            dialogOpenSources=['Native Browse button (cancel)','Registered path.choose operation (file/directory selection and cancel)'],
            tools=True,singleOnlySelects=True,doubleFrames=True,fFrames=True,rightLook=True,flight=True,flightRelease=True,outsideRelease=True,focusRelease=True,resize=True,rawMouse=raw_mouse,
            nativeFileChoose=True,nativeDirectoryChoose=True,nativeCancel=True,utf8Path=True,contentPreserved=True)
        (output/'summary.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
        return summary
    finally:
        if 'original_cursor' in locals():window.user.SetCursorPos(original_cursor.x,original_cursor.y)
        if process.poll() is None:process.terminate();process.wait(timeout=5)
        for log in logs:log.close()
        (output/'events.json').write_text(json.dumps(evidence,indent=2),encoding='utf-8')


if __name__=='__main__':
    assert ctypes.WinDLL('user32').SetProcessDPIAware()
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--executable',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();root=args.output.resolve()/('run-'+str(time.time_ns()))
    results=[run(args.executable.resolve(),root/workflow,workflow) for workflow in ('exploration','inspector')]
    (root/'summary.json').write_text(json.dumps(results,indent=2),encoding='utf-8');print(json.dumps(results))
