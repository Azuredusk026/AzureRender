"""Windows surface lifecycle smoke test; operates only on its child process."""
import argparse
import ctypes as c, subprocess, time, pathlib
from ctypes import wintypes as w
u=c.windll.user32
u.IsWindowVisible.argtypes=[w.HWND]
u.GetWindowThreadProcessId.argtypes=[w.HWND,c.POINTER(w.DWORD)]
u.ShowWindow.argtypes=[w.HWND,c.c_int]
u.MoveWindow.argtypes=[w.HWND,c.c_int,c.c_int,c.c_int,c.c_int,w.BOOL]
u.PostMessageW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM]
callback=c.WINFUNCTYPE(w.BOOL,w.HWND,w.LPARAM)
parser=argparse.ArgumentParser()
parser.add_argument('--executable', default='build/ninja-msvc-debug/AzureRender.exe')
args=parser.parse_args()
root=pathlib.Path(__file__).resolve().parents[1]
executable=pathlib.Path(args.executable).resolve()
for mode in ('restore','close-minimized'):
    log=root/'build'/f'r1-window-{mode}.log'
    with log.open('w') as f:
        p=subprocess.Popen([str(executable),'--scene-type','sample'],stdout=f,stderr=f,cwd=root)
        try:
            windows=[]
            @callback
            def visit(hwnd,_):
                pid=w.DWORD(); u.GetWindowThreadProcessId(hwnd,c.byref(pid))
                if pid.value==p.pid and u.IsWindowVisible(hwnd): windows.append(hwnd)
                return True
            deadline=time.monotonic()+20
            while not windows and time.monotonic()<deadline:
                u.EnumWindows(visit,0); time.sleep(.1)
            assert windows,'no renderer window'
            hwnd=windows[0]; time.sleep(2)
            u.MoveWindow(hwnd,50,50,900,650,True); time.sleep(1)
            u.ShowWindow(hwnd,6); time.sleep(1)
            if mode=='restore':
                u.ShowWindow(hwnd,9); time.sleep(2)
                u.MoveWindow(hwnd,50,50,1100,750,True); time.sleep(1)
            assert p.poll() is None,'renderer exited prematurely'
            u.PostMessageW(hwnd,0x10,0,0)
            assert p.wait(timeout=20)==0,'renderer failed'
        finally:
            if p.poll() is None: p.kill(); p.wait()
    text=log.read_text(errors='replace')
    assert 'Validation Error' not in text and 'VUID-' not in text,text[-3000:]
    print(mode,'passed',log)
