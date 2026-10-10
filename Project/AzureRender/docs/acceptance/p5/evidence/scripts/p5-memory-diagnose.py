"""Classify actual resident pages while repeating the editor workload."""
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import shutil
import sys
import time
ROOT=Path.cwd().resolve();WORK=ROOT/'build/implementation-2026-10-10'
sys.path.insert(0,str(ROOT/'tools'))
import run_playable_performance as measurement
OUT=WORK/'p5-memory-diagnose';OUT.mkdir(exist_ok=False)
shutil.copytree(WORK/'p5-performance/editor',OUT/'editor',ignore=shutil.ignore_patterns('.azure'))
class Counters(ctypes.Structure):
    _fields_=[('cb',ctypes.c_ulong),('faults',ctypes.c_ulong)]+[(n,ctypes.c_size_t) for n in ('peak','working','quotaPeakPaged','quotaPaged','quotaPeakNonPaged','quotaNonPaged','pagefile','peakPagefile')]
class Region(ctypes.Structure):
    _fields_=[('base',ctypes.c_void_p),('allocation',ctypes.c_void_p),('allocationProtect',ctypes.c_ulong),('size',ctypes.c_size_t),('state',ctypes.c_ulong),('protect',ctypes.c_ulong),('kind',ctypes.c_ulong)]
kernel=ctypes.WinDLL('kernel32',use_last_error=True);kernel.OpenProcess.restype=ctypes.c_void_p
kernel.CloseHandle.argtypes=[ctypes.c_void_p]
kernel.VirtualQueryEx.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t]
psapi=ctypes.WinDLL('psapi');psapi.GetProcessMemoryInfo.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_ulong]
psapi.QueryWorkingSet.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_ulong]
psapi.GetMappedFileNameW.argtypes=[ctypes.c_void_p,ctypes.c_void_p,wintypes.LPWSTR,ctypes.c_ulong]
snapshots=[];last=0
def snapshot(handle,data):
    buffer=(ctypes.c_size_t*(1024*1024))()
    assert psapi.QueryWorkingSet(handle,buffer,ctypes.sizeof(buffer)),ctypes.get_last_error()
    pages=sorted((int(buffer[i])>>12)<<12 for i in range(1,int(buffer[0])+1))
    regions={};upper=0;row=None
    for address in pages:
        if address>=upper:
            region=Region();assert kernel.VirtualQueryEx(handle,address,ctypes.byref(region),ctypes.sizeof(region))
            upper=region.base+region.size
            key=(region.allocation,region.kind)
            if key not in regions:
                path=ctypes.create_unicode_buffer(1024)
                psapi.GetMappedFileNameW(handle,address,path,len(path))
                regions[key]=dict(allocation=region.allocation,kind=region.kind,path=path.value,residentBytes=0,regionBytes=0)
            row=regions[key];row['regionBytes']+=region.size
        row['residentBytes']+=4096
    snapshots.append(dict(seconds=time.monotonic(),working=data.working,privateBytes=data.pagefile,
        regions=sorted(regions.values(),key=lambda r:r['residentBytes'],reverse=True)))
def memory(pid):
    global last
    handle=kernel.OpenProcess(0x410,False,pid)
    if not handle:return 0
    try:
        data=Counters();data.cb=ctypes.sizeof(data)
        if not psapi.GetProcessMemoryInfo(handle,ctypes.byref(data),data.cb):return 0
        if time.monotonic()-last>1:
            snapshot(handle,data);last=time.monotonic()
        return data.peak
    finally:kernel.CloseHandle(handle)
measurement.memory=memory
for index in range(2):
    snapshots.clear();last=0
    os.environ['AZURERENDER_EDITOR_CONFIG']=str(OUT/f'config-{index}')
    result=measurement.run(ROOT/'build/ninja-msvc-release/AzureRender.exe',OUT/'editor/project.azureproject',OUT/f'editor-{index}',True,False)
    (OUT/f'pages-{index}.json').write_text(json.dumps(snapshots,indent=2)+'\n',encoding='utf-8')
    print('diagnostic',index,'resident snapshots',len(snapshots),flush=True)
