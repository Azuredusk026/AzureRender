"""Diagnose an owned process; resident pages do not prove allocation ownership."""
import argparse
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import struct
import subprocess
import time


class AllocationLedger:
    """Pair successful whole-region events within explicitly limited coverage."""
    def __init__(self):
        self.live={}
        self.ambiguous_events=0

    @property
    def live_bytes(self):
        return sum(self.live.values())

    def observe(self,event):
        if not event['success']:return
        base=event['base']
        if event['action']=='allocate':
            if base in self.live:self.ambiguous_events+=1
            else:self.live[base]=event['size']
        elif event['action']=='release':
            if base not in self.live:self.ambiguous_events+=1
            else:del self.live[base]
        else:self.ambiguous_events+=1


def summarize_residency(previous,current):
    def index(snapshot):
        return {(r['allocation'],r['kind'],r['path']):r for r in snapshot['regions']}
    old,new=index(previous),index(current)
    classes={};rows=[]
    for key in sorted(old.keys()|new.keys(),key=lambda k:(k[0] or 0,k[1],k[2])):
        before=old.get(key,{}).get('residentBytes',0)
        after=new.get(key,{}).get('residentBytes',0)
        delta=after-before
        if not delta:continue
        base,kind,path=key
        category={0x1000000:'image',0x40000:'mapped',0x20000:'privateUnattributed'}.get(kind,'unknown')
        classes[category]=classes.get(category,0)+delta
        rows.append(dict(allocation=base,kind=kind,path=path,beforeResidentBytes=before,
            afterResidentBytes=after,changeBytes=delta,category=category))
    return dict(byClass=classes,regions=sorted(rows,key=lambda r:abs(r['changeBytes']),reverse=True),
        netResidentChangeBytes=sum(classes.values()),allocationOwnershipProven=False,
        allocationReleaseProof=[])


class ResidentObserver:
    def __init__(self,pid):
        self.pid=pid
        self.kernel=ctypes.WinDLL('kernel32',use_last_error=True)
        self.psapi=ctypes.WinDLL('psapi',use_last_error=True)
        self.kernel.OpenProcess.restype=ctypes.c_void_p
        self.kernel.CloseHandle.argtypes=[ctypes.c_void_p]
        self.kernel.VirtualQueryEx.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t]
        self.kernel.VirtualQueryEx.restype=ctypes.c_size_t
        self.psapi.QueryWorkingSet.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_ulong]
        self.psapi.GetMappedFileNameW.argtypes=[ctypes.c_void_p,ctypes.c_void_p,wintypes.LPWSTR,ctypes.c_ulong]
        self.handle=self.kernel.OpenProcess(0x410,False,pid)
        if not self.handle:raise OSError(ctypes.get_last_error(),'OpenProcess failed')
        info=ctypes.create_string_buffer(64)
        self.kernel.GetSystemInfo(ctypes.byref(info))
        self.page_size=struct.unpack_from('<I',info.raw,4)[0]

    def close(self):
        if self.handle:self.kernel.CloseHandle(self.handle);self.handle=None

    def snapshot(self):
        class Region(ctypes.Structure):
            _fields_=[('base',ctypes.c_void_p),('allocation',ctypes.c_void_p),
                ('allocationProtect',ctypes.c_ulong),('size',ctypes.c_size_t),
                ('state',ctypes.c_ulong),('protect',ctypes.c_ulong),('kind',ctypes.c_ulong)]
        count=16384
        while count<=4*1024*1024:
            buffer=(ctypes.c_size_t*count)()
            if self.psapi.QueryWorkingSet(self.handle,buffer,ctypes.sizeof(buffer)):
                if buffer[0]>=count:raise ValueError('Truncated working-set page list')
                break
            error=ctypes.get_last_error()
            if error!=24:raise OSError(error,'QueryWorkingSet failed')
            count*=2
        else:raise ValueError('Working-set page list exceeds diagnostic capacity')
        pages=sorted((int(buffer[i])>>12)<<12 for i in range(1,int(buffer[0])+1))
        regions={};upper=0;row=None;raced_pages=0
        for address in pages:
            if address>=upper:
                region=Region()
                if not self.kernel.VirtualQueryEx(self.handle,address,ctypes.byref(region),ctypes.sizeof(region)):
                    raise OSError(ctypes.get_last_error(),'VirtualQueryEx failed')
                upper=region.base+region.size
                if region.state!=0x1000 or region.allocation is None:
                    row=None;raced_pages+=1;continue
                path=ctypes.create_unicode_buffer(2048)
                if region.kind in (0x1000000,0x40000):
                    self.psapi.GetMappedFileNameW(self.handle,address,path,len(path))
                key=(region.allocation,region.kind,path.value)
                if key not in regions:
                    regions[key]=dict(allocation=region.allocation,kind=region.kind,path=path.value,
                        residentBytes=0,queriedRegionBytes=0)
                row=regions[key];row['queriedRegionBytes']+=region.size
            if row:row['residentBytes']+=self.page_size
            else:raced_pages+=1
        return dict(monotonicSeconds=time.monotonic(),pid=self.pid,pageSize=self.page_size,
            residentPages=len(pages)-raced_pages,racedPages=raced_pages,
            regions=list(regions.values()),diagnosticOnly=True)


def self_test(output):
    output.mkdir(parents=True,exist_ok=False)
    kernel=ctypes.WinDLL('kernel32',use_last_error=True)
    kernel.VirtualAlloc.argtypes=[ctypes.c_void_p,ctypes.c_size_t,ctypes.c_ulong,ctypes.c_ulong]
    kernel.VirtualAlloc.restype=ctypes.c_void_p
    kernel.VirtualFree.argtypes=[ctypes.c_void_p,ctypes.c_size_t,ctypes.c_ulong]
    observer=ResidentObserver(os.getpid());ledger=AllocationLedger();events=[]
    pointer=None
    try:
        size=4*1024*1024
        pointer=kernel.VirtualAlloc(None,size,0x3000,4)
        event=dict(action='allocate',success=bool(pointer),base=pointer,size=size)
        events.append(event);ledger.observe(event)
        if not pointer:raise OSError(ctypes.get_last_error(),'VirtualAlloc self-test failed')
        ctypes.memset(pointer,0x71,size)
        resident=observer.snapshot()
        target=next(r for r in resident['regions'] if r['allocation']==pointer)
        assert target['kind']==0x20000 and target['residentBytes']==size,target
        freed=bool(kernel.VirtualFree(pointer,0,0x8000))
        event=dict(action='release',success=freed,base=pointer,size=0)
        events.append(event);ledger.observe(event)
        if not freed:raise OSError(ctypes.get_last_error(),'VirtualFree self-test failed')
        released_pointer=pointer;pointer=None
        released=observer.snapshot()
        assert all(r['allocation']!=released_pointer for r in released['regions'])
        assert ledger.live_bytes==0 and ledger.ambiguous_events==0
        result=dict(passed=True,diagnosticOnly=True,coverage='owned 4 MiB whole-region allocation and release',
            events=events,resident=resident,released=released,liveBytes=ledger.live_bytes,
            limits=['Self-test does not trace allocations in the engine or driver.'])
        (output/'self-test.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
        print(json.dumps(dict(passed=True,residentBytes=target['residentBytes'],liveBytes=ledger.live_bytes)))
    finally:
        if pointer:kernel.VirtualFree(pointer,0,0x8000)
        observer.close()


def collect(command,output,interval,timeout):
    from run_playable_performance import memory_snapshot
    if not command:raise ValueError('An owned child command is required')
    output.mkdir(parents=True,exist_ok=False)
    observations=[];pages=[];errors=[];observer=None
    start=time.monotonic();last=-interval
    info=subprocess.STARTUPINFO();info.dwFlags|=subprocess.STARTF_USESHOWWINDOW;info.wShowWindow=0
    with (output/'stdout.log').open('w',encoding='utf-8') as stdout,(output/'stderr.log').open('w',encoding='utf-8') as stderr:
        process=subprocess.Popen(command,stdout=stdout,stderr=stderr,startupinfo=info)
        try:
            observer=ResidentObserver(process.pid)
            while process.poll() is None:
                now=time.monotonic()-start
                sample=memory_snapshot(process.pid);sample['seconds']=now
                observations.append(sample)
                if now-last>=interval:
                    pages.append(observer.snapshot());last=now
                if now>timeout:raise TimeoutError('Owned diagnostic process exceeded timeout')
                time.sleep(.05)
        except Exception as error:
            errors.append(dict(type=type(error).__name__,message=str(error)))
        finally:
            if process.poll() is None:process.kill();process.wait()
            if observer:observer.close()
    result=dict(diagnosticOnly=True,command=command,exitCode=process.returncode,errors=errors,
        observations=observations,snapshots=pages,
        coverage='resident regions, process counters; allocation call stacks require separate tracing')
    if len(pages)>1:result['residentChange']=summarize_residency(pages[0],pages[-1])
    (output/'residency.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    return 0 if not errors and process.returncode==0 else 1


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('--output',required=True,type=Path)
    parser.add_argument('--self-test',action='store_true')
    parser.add_argument('--interval',type=float,default=1)
    parser.add_argument('--timeout',type=float,default=600)
    parser.add_argument('command',nargs=argparse.REMAINDER)
    args=parser.parse_args()
    if args.interval<=0 or args.timeout<=0:parser.error('Interval and timeout must be positive')
    if args.self_test:self_test(args.output)
    else:raise SystemExit(collect(args.command[1:] if args.command[:1]==['--'] else args.command,
        args.output,args.interval,args.timeout))
