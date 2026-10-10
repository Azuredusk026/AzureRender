"""Observe CPU work and instruction modules of this diagnostic's own child threads."""
import ctypes, json, os, subprocess, sys, time
from ctypes import wintypes
from pathlib import Path
ROOT=Path.cwd();WORK=ROOT/'build/implementation-2026-10-10';OUT=WORK/'p5-thread-diagnose'
OUT.mkdir(exist_ok=False);sys.path.insert(0,str(ROOT/'tools'))
import run_playable_performance as measurement
k=ctypes.WinDLL('kernel32',use_last_error=True);p=ctypes.WinDLL('psapi',use_last_error=True)
class Thread(ctypes.Structure):
 _fields_=[('size',wintypes.DWORD),('usage',wintypes.DWORD),('tid',wintypes.DWORD),('pid',wintypes.DWORD),('priority',wintypes.LONG),('delta',wintypes.LONG),('flags',wintypes.DWORD)]
k.CreateToolhelp32Snapshot.restype=wintypes.HANDLE;k.OpenThread.restype=wintypes.HANDLE;k.OpenProcess.restype=wintypes.HANDLE
k.CloseHandle.argtypes=[wintypes.HANDLE];k.Thread32First.argtypes=[wintypes.HANDLE,ctypes.POINTER(Thread)];k.Thread32Next.argtypes=k.Thread32First.argtypes
k.SuspendThread.argtypes=[wintypes.HANDLE];k.ResumeThread.argtypes=[wintypes.HANDLE];k.GetThreadContext.argtypes=[wintypes.HANDLE,ctypes.c_void_p]
k.GetThreadTimes.argtypes=[wintypes.HANDLE,*([ctypes.POINTER(ctypes.c_ulonglong)]*4)]
p.GetMappedFileNameW.argtypes=[wintypes.HANDLE,ctypes.c_void_p,wintypes.LPWSTR,wintypes.DWORD]
base_memory=measurement.memory;begin=None;last=0;rows=[]
def sample(pid,elapsed):
 handle=k.OpenProcess(0x410,False,pid);snap=k.CreateToolhelp32Snapshot(4,0);e=Thread();e.size=ctypes.sizeof(e)
 threads=[]
 try:
  ok=k.Thread32First(snap,ctypes.byref(e))
  while ok:
   if e.pid==pid:
    th=k.OpenThread(0x4a,False,e.tid)
    if th:
     try:
      times=[ctypes.c_ulonglong() for _ in range(4)];success=k.GetThreadTimes(th,*[ctypes.byref(t) for t in times])
      raw=ctypes.create_string_buffer(1250);addr=(ctypes.addressof(raw)+15)&~15;ctypes.c_uint32.from_address(addr+48).value=0x100001
      pc=0;path=ctypes.create_unicode_buffer(1024)
      if k.SuspendThread(th)!=0xffffffff:
       try:
        if k.GetThreadContext(th,addr):pc=ctypes.c_ulonglong.from_address(addr+248).value
       finally:k.ResumeThread(th)
      if pc and handle:p.GetMappedFileNameW(handle,pc,path,len(path))
      threads.append(dict(tid=e.tid,userMs=times[3].value/10000,kernelMs=times[2].value/10000,instruction=pc,module=path.value,timesValid=bool(success)))
     finally:k.CloseHandle(th)
   ok=k.Thread32Next(snap,ctypes.byref(e))
 finally:
  k.CloseHandle(snap)
  if handle:k.CloseHandle(handle)
 rows.append(dict(seconds=elapsed,threads=threads))
def memory(pid):
 global begin,last
 now=time.monotonic()
 if begin is None:begin=now
 if now-last>=.25:sample(pid,now-begin);last=now
 return base_memory(pid)
measurement.memory=memory
os.environ['AZURERENDER_EDITOR_CONFIG']=str(OUT/'config')
result=measurement.run(ROOT/'build/ninja-msvc-release/AzureRender.exe',WORK/'p5-performance/editor/project.azureproject',OUT/'editor',True,False)
(OUT/'threads.json').write_text(json.dumps(rows,indent=2))
(OUT/'result.json').write_text(json.dumps(result,indent=2))
print('own-child thread samples',len(rows),flush=True)
