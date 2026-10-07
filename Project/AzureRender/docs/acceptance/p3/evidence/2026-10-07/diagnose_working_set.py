import ctypes, json, sys, time
from pathlib import Path
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools'))
import run_playable_performance as perf
class Counters(ctypes.Structure):
    _fields_=[('cb',ctypes.c_ulong),('faults',ctypes.c_ulong)]+[(n,ctypes.c_size_t) for n in ('peak','working','quotaPeakPaged','quotaPaged','quotaPeakNonPaged','quotaNonPaged','pagefile','peakPagefile','private')]
class Region(ctypes.Structure):
    _fields_=[('base',ctypes.c_void_p),('allocationBase',ctypes.c_void_p),('allocationProtect',ctypes.c_ulong),('partition',ctypes.c_ushort),('size',ctypes.c_size_t),('state',ctypes.c_ulong),('protect',ctypes.c_ulong),('type',ctypes.c_ulong)]
k=ctypes.WinDLL('kernel32');k.OpenProcess.restype=ctypes.c_void_p;k.CloseHandle.argtypes=[ctypes.c_void_p]
k.VirtualQueryEx.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t];k.VirtualQueryEx.restype=ctypes.c_size_t
p=ctypes.WinDLL('psapi');p.GetProcessMemoryInfo.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_ulong]
rows=[];start=0
def memory(pid):
    h=k.OpenProcess(0x410,False,pid)
    if not h:return 0
    try:
        c=Counters();c.cb=ctypes.sizeof(c)
        if not p.GetProcessMemoryInfo(h,ctypes.byref(c),c.cb):return 0
        row={'seconds':time.monotonic()-start,**{n:getattr(c,n) for n in ('working','private','pagefile','peak','faults')}}
        if not rows or abs(row['working']-rows[-1]['working'])>8*1024**2:
            totals={};address=0;large=[]
            while address<0x7fffffffffff:
                r=Region()
                if not k.VirtualQueryEx(h,ctypes.c_void_p(address),ctypes.byref(r),ctypes.sizeof(r)):break
                if r.state==0x1000:
                    key=f'{r.type:x}:{r.protect:x}';totals[key]=totals.get(key,0)+r.size
                    if r.size>=8*1024**2:large.append({'base':r.base,'size':r.size,'type':r.type,'protect':r.protect})
                address=(r.base or 0)+r.size
            row['committedRegions']=totals;row['largeRegions']=large
        rows.append(row);return c.peak
    finally:k.CloseHandle(h)
perf.memory=memory
out=root/'build/evolution/p3/memory-observation-2';out.mkdir(exist_ok=False)
project=root/'build/evolution/p3/performance/stress/project.azureproject'
for i in range(5):
    rows.clear();start=time.monotonic()
    result=perf.run(root/'build/ninja-msvc-release/AzurePlayer.exe',project,out/f'stress-{i+1}',stress=True)
    (out/f'stress-{i+1}.memory.json').write_text(json.dumps({'result':result,'observations':rows},indent=2)+'\n')
    a=rows[len(rows)//2:]
    print('MEMORY',i+1,'growth',max(r['working'] for r in a)/a[0]['working']-1,'private growth',max(r['private'] for r in a)-a[0]['private'],flush=True)
