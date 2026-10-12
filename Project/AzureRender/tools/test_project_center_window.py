"""Exercise the real no-argument window, resize and close through native messages."""
import argparse, ctypes, json, os, subprocess, time
from ctypes import wintypes
from pathlib import Path
from PIL import ImageGrab
from run_playable_long_run import NativeWindow

def run(executable, output):
    output=output.resolve();output.mkdir(parents=True,exist_ok=True)
    env=os.environ.copy();env['AZURERENDER_EDITOR_CONFIG']=str(output/'config')
    with (output/'window.log').open('w',encoding='utf-8') as log:
        process=subprocess.Popen([str(executable.resolve())],cwd=output,env=env,stdout=log,stderr=log)
        try:
            window=NativeWindow(process.pid);deadline=time.monotonic()+15
            while not window.find():
                assert process.poll() is None,'Center exited before window initialization'
                assert time.monotonic()<deadline,'No project center window'
                time.sleep(.1)
            time.sleep(3)
            rect=wintypes.RECT();window.user.GetWindowRect(window.handle,ctypes.byref(rect))
            ImageGrab.grab((rect.left,rect.top,rect.right,rect.bottom)).save(output/'project-center.png')
            window.resize(960,640);time.sleep(.5);window.close()
            assert process.wait(timeout=15)==0,'Window close must exit normally'
        finally:
            if process.poll() is None:process.terminate();process.wait(timeout=10)
    text=(output/'window.log').read_text(encoding='utf-8')
    assert '[validation warning]' not in text and 'VUID-' not in text,text[-4000:]
    result={'noArgumentWindow':True,'resize':True,'nativeClose':True,'validationErrors':False,'cwd':'unrelated directory','physicalKeyboardMouse':'not performed'}
    (output/'summary.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--executable',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args();run(args.executable,args.output)
