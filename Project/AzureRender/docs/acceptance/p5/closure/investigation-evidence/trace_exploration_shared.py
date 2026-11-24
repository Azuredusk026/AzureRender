from pathlib import Path
import ctypes,json,subprocess,sys,time
from ctypes import wintypes
sys.path.insert(0,str(Path('tools').resolve()))
from collect_memory_residency import ResidentObserver
from run_playable_performance import memory_snapshot
root=Path.cwd();out=root/'build/p5-p6-closure-2026-10-10';traceout=out/'exploration-return-trace-shared'
command=[sys.executable,'tools/trace_virtual_allocations.py','--output',str(traceout),'--timeout','600','--',
    'build/ninja-msvc-release-symbols/AzurePlayer.exe','--project',str(out/'formal-source-verified/exploration/project.azureproject'),
    '--width','1920','--height','1080','--fixed-frame-step','--smoke-frames','2100',
    '--game-actions',str(out/'formal-source-verified/exploration-1.input.json'),
    '--gpu-timing','--gpu-timing-output',str(out/'exploration-trace-shared-timing.json')]
kernel=ctypes.WinDLL('kernel32',use_last_error=True)
class Entry(ctypes.Structure):
    _fields_=[('size',wintypes.DWORD),('usage',wintypes.DWORD),('pid',wintypes.DWORD),('heap',ctypes.c_size_t),
        ('module',wintypes.DWORD),('threads',wintypes.DWORD),('parent',wintypes.DWORD),('priority',wintypes.LONG),
        ('flags',wintypes.DWORD),('exe',wintypes.WCHAR*260)]
kernel.CreateToolhelp32Snapshot.restype=wintypes.HANDLE
kernel.Process32FirstW.argtypes=[wintypes.HANDLE,ctypes.POINTER(Entry)]
kernel.Process32NextW.argtypes=[wintypes.HANDLE,ctypes.POINTER(Entry)]
kernel.CloseHandle.argtypes=[wintypes.HANDLE]
def processes():
    handle=kernel.CreateToolhelp32Snapshot(2,0);e=Entry();e.size=ctypes.sizeof(e);rows=[]
    try:
        more=kernel.Process32FirstW(handle,ctypes.byref(e))
        while more:
            rows.append((e.pid,e.parent,e.exe));more=kernel.Process32NextW(handle,ctypes.byref(e))
    finally:kernel.CloseHandle(handle)
    return rows
rows=[];errors=[];snapshots=[];observer=None;start=time.monotonic();last=-1
with (out/'exploration-trace-shared-run.log').open('w') as log:
    process=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT)
    while process.poll() is None:
        if observer is None:
            table=processes();lldbs={pid for pid,parent,name in table if parent==process.pid and name.lower()=='lldb.exe'}
            children=[pid for pid,parent,name in table if parent in lldbs and name.lower()=='azureplayer.exe']
            if children:observer=ResidentObserver(children[0])
        if observer:
            try:
                now=time.monotonic()-start;sample=memory_snapshot(observer.pid);sample['seconds']=now;rows.append(sample)
                if now-last>=1:
                    s=observer.snapshot();s['seconds']=now;s['counters']=sample;snapshots.append(s);last=now
            except OSError as e:
                errors.append(dict(seconds=time.monotonic()-start,error=str(e),mayBeProcessExit=True))
                observer.close();observer=None
        (out/'exploration-trace-shared-residency.json').write_text(json.dumps(dict(diagnosticOnly=True,
            observations=rows,snapshots=snapshots,errors=errors),indent=2)+'\n')
        time.sleep(.05)
    if observer:observer.close()
    print('trace exit',process.returncode,'snapshots',len(snapshots))
    raise SystemExit(process.returncode)
