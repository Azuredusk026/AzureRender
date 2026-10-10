from pathlib import Path
import sys,json,threading,ctypes
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0,str(Path('tools').resolve()))
out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=False)
barrier=threading.Barrier(8)
def probe(index):
    tid=threading.get_native_id();barrier.wait()
    dll=ctypes.WinDLL('ntdll');ptr=ctypes.POINTER(ctypes.c_void_p);sizeptr=ctypes.POINTER(ctypes.c_size_t)
    dll.NtAllocateVirtualMemory.argtypes=[ctypes.c_void_p,ptr,ctypes.c_size_t,sizeptr,ctypes.c_ulong,ctypes.c_ulong]
    dll.NtFreeVirtualMemory.argtypes=[ctypes.c_void_p,ptr,sizeptr,ctypes.c_ulong]
    events=[]
    for size in (4194304,1048576):
        base=ctypes.c_void_p();extent=ctypes.c_size_t(size)
        status=dll.NtAllocateVirtualMemory(ctypes.c_void_p(-1),ctypes.byref(base),0,ctypes.byref(extent),0x3000,4)&0xffffffff
        assert status==0;allocated=base.value
        events.append(dict(api='NtAllocateVirtualMemory',action='allocate',base=allocated,size=size,status=status,flags=0x3000))
        ctypes.memset(allocated,0x71,size);extent=ctypes.c_size_t(0)
        status=dll.NtFreeVirtualMemory(ctypes.c_void_p(-1),ctypes.byref(base),ctypes.byref(extent),0x8000)&0xffffffff
        assert status==0
        events.append(dict(api='NtFreeVirtualMemory',action='release',base=allocated,size=0,status=status,flags=0x8000))
    base=ctypes.c_void_p();extent=ctypes.c_size_t(0)
    status=dll.NtAllocateVirtualMemory(ctypes.c_void_p(-1),ctypes.byref(base),0,ctypes.byref(extent),0x3000,4)&0xffffffff
    assert status&0x80000000
    events.append(dict(api='NtAllocateVirtualMemory',action='allocate',base=0,size=0,status=status,flags=0x3000))
    return dict(threadId=tid,proof=dict(events=events))
with ThreadPoolExecutor(max_workers=8) as pool:rows=list(pool.map(probe,range(8)))
(out/'threads.json').write_text(json.dumps(rows,indent=2)+'\n')
