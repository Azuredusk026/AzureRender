"""Successful return events, rather than requests, determine trace results."""
import unittest
import json
from pathlib import Path
import sys
from trace_virtual_allocations import summarize_events


class ReturnedAllocationTests(unittest.TestCase):
    def test_successful_allocation_release_and_failed_requests(self):
        events=[dict(action='allocate',status=0,base=4096,size=8192,flags=0x3000),
            dict(action='allocate',status=0xc0000017,base=0,size=8192,flags=0x3000),
            dict(action='release',status=0xc0000001,base=4096,size=0,flags=0x8000),
            dict(action='release',status=0,base=4096,size=0,flags=0x8000)]
        result=summarize_events(events)
        self.assertEqual(result['successfulAllocations'],1)
        self.assertEqual(result['successfulReleases'],1)
        self.assertEqual(result['failedCalls'],2)
        self.assertEqual(result['trackedLiveBytes'],0)

    def test_decommit_and_nested_commit_are_not_whole_region_release(self):
        events=[dict(action='allocate',status=0,base=4096,size=8192,flags=0x3000),
            dict(action='allocate',status=0,base=4096,size=4096,flags=0x1000),
            dict(action='release',status=0,base=4096,size=4096,flags=0x4000)]
        result=summarize_events(events)
        self.assertEqual(result['trackedLiveBytes'],8192)
        self.assertEqual(result['partialOrAmbiguousCalls'],2)
        self.assertFalse(result['completeAllocationCoverage'])


def native_probe(output):
    """Known local NT calls used for an optional, real debugger verification."""
    import ctypes
    dll=ctypes.WinDLL('ntdll')
    pointer_type=ctypes.POINTER(ctypes.c_void_p)
    size_type=ctypes.POINTER(ctypes.c_size_t)
    dll.NtAllocateVirtualMemory.argtypes=[ctypes.c_void_p,pointer_type,ctypes.c_size_t,size_type,ctypes.c_ulong,ctypes.c_ulong]
    dll.NtAllocateVirtualMemoryEx.argtypes=[ctypes.c_void_p,pointer_type,size_type,ctypes.c_ulong,ctypes.c_ulong,ctypes.c_void_p,ctypes.c_ulong]
    dll.NtFreeVirtualMemory.argtypes=[ctypes.c_void_p,pointer_type,size_type,ctypes.c_ulong]
    for api in ('NtAllocateVirtualMemory','NtAllocateVirtualMemoryEx','NtFreeVirtualMemory'):
        getattr(dll,api).restype=ctypes.c_long
    process=ctypes.c_void_p(-1);events=[]
    for api,requested in (('NtAllocateVirtualMemory',4194304),('NtAllocateVirtualMemoryEx',1048576)):
        base=ctypes.c_void_p();size=ctypes.c_size_t(requested)
        if api=='NtAllocateVirtualMemory':
            status=dll.NtAllocateVirtualMemory(process,ctypes.byref(base),0,ctypes.byref(size),0x3000,4)&0xffffffff
        else:
            status=dll.NtAllocateVirtualMemoryEx(process,ctypes.byref(base),ctypes.byref(size),0x3000,4,None,0)&0xffffffff
        assert not status & 0x80000000 and base.value and size.value==requested
        allocated=base.value
        events.append(dict(api=api,action='allocate',base=allocated,size=size.value,status=status,flags=0x3000))
        try:
            ctypes.memset(allocated,0x71,requested)
        finally:
            size=ctypes.c_size_t(0)
            status=dll.NtFreeVirtualMemory(process,ctypes.byref(base),ctypes.byref(size),0x8000)&0xffffffff
            events.append(dict(api='NtFreeVirtualMemory',action='release',base=allocated,size=0,status=status,flags=0x8000))
            assert not status & 0x80000000
        # A second release must fail and must not delete another ledger entry.
        base=ctypes.c_void_p(allocated);size=ctypes.c_size_t(0)
        status=dll.NtFreeVirtualMemory(process,ctypes.byref(base),ctypes.byref(size),0x8000)&0xffffffff
        assert status & 0x80000000
        events.append(dict(api='NtFreeVirtualMemory',action='release',base=allocated,size=0,status=status,flags=0x8000))
    base=ctypes.c_void_p();size=ctypes.c_size_t(0)
    status=dll.NtAllocateVirtualMemory(process,ctypes.byref(base),0,ctypes.byref(size),0x3000,4)&0xffffffff
    assert status & 0x80000000
    events.append(dict(api='NtAllocateVirtualMemory',action='allocate',base=base.value or 0,size=0,status=status,flags=0x3000))
    Path(output).write_text(json.dumps(dict(passed=True,diagnosticOnly=True,events=events),indent=2)+'\n',encoding='utf-8')


def verify_probe(probe_path, trace_directory, output):
    folder=Path(trace_directory)
    state=json.loads((folder/'summary.json').read_text(encoding='utf-8'))
    proof=json.loads(Path(probe_path).read_text(encoding='utf-8'))
    events=[json.loads(line) for line in (folder/'events.jsonl').read_text(encoding='utf-8').splitlines()]
    matched=[];cursor=0
    for expected in proof['events']:
        found=next((i for i in range(cursor,len(events)) if all(
            events[i][key]==expected[key] for key in ('action','api','status','base','size','flags'))),None)
        assert found is not None,expected
        matched.append(events[found]);cursor=found+1
    summary=summarize_events(matched)
    assert summary['successfulAllocations']==2 and summary['successfulReleases']==2
    assert summary['failedCalls']==3 and summary['trackedLiveBytes']==0
    assert not state['errors'] and not state['pendingReturns'] and not state['droppedEntries']
    assert state['entryCalls']==state['returnedEvents']==len(events) and state['commandExitCode']==0
    assert all(state['resolvedApis'].values())
    result=dict(passed=True,diagnosticOnly=True,probeEvents=len(matched),traceEvents=len(events),
        probeSummary=summary,unmatchedCalls=len(events)-len(matched),sourceProof=str(probe_path),trace=str(folder))
    Path(output).write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(result))


if __name__=='__main__':
    if sys.argv[1:2]==['--probe']:native_probe(sys.argv[2])
    elif sys.argv[1:2]==['--verify-probe']:verify_probe(*sys.argv[2:])
    else:unittest.main()
