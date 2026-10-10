"""Summarize diagnostic requests without treating them as leaked allocations."""
import collections,json,re,sys
from pathlib import Path
WORK=Path.cwd()/'build/implementation-2026-10-10'
OUT=WORK/(sys.argv[1] if len(sys.argv)>1 else 'p5-allocation-debug-native-entry')
raw=(OUT/'debugger.log').read_text(encoding='utf-8')
observations=json.loads((OUT/'memory.json').read_text(encoding='utf-8'))
stats=collections.defaultdict(lambda:dict(requests=0,requestedBytes=0,maxRequestBytes=0,examples=[]))
positions=list(re.finditer(r'\(lldb\).*? register read rcx rdx r8 r9 rsp',raw))
for index,start in enumerate(positions):
    text=raw[start.start():positions[index+1].start() if index+1<len(positions) else len(raw)]
    size=re.search(r'memory read[^\n]+\$r9\n0x[0-9a-f]+: 0x([0-9a-f]+)',text)
    base=re.search(r'memory read[^\n]+\$rdx\n0x[0-9a-f]+: 0x([0-9a-f]+)',text)
    frames=re.findall(r'frame #\d+: 0x[0-9a-f]+ ([^`\r\n]+)`[^\n]*',text)
    if not size:continue
    caller=next((m for m in frames if m.lower() not in ('ntdll.dll','kernelbase.dll','ucrtbase.dll')),'unattributed')
    amount=int(size[1],16)
    row=stats[caller];row['requests']+=1;row['requestedBytes']+=amount;row['maxRequestBytes']=max(row['maxRequestBytes'],amount)
    if len(row['examples'])<3:
        row['examples'].append(dict(size=amount,requestedAddress=int(base[1],16) if base else None,frames=frames))
stable=observations[len(observations)//2:]
summary=dict(diagnosticOnly=True,requestCount=len(positions),byFirstNonSystemCaller=stats,
             sampledWorkingSetGrowth=max(r['workingSet'] for r in stable)/stable[0]['workingSet']-1,
             sampledPrivateGrowth=max(r['privateBytes'] for r in stable)/stable[0]['privateBytes']-1,
             memoryObservations=len(observations),completion=json.loads((OUT/'summary.json').read_text()),
             limitations=['Requests do not prove successful allocations, current residency, or leaks.',
                          'Debugger timing and heaps differ from formal runs.',
                          'NtAllocateVirtualMemoryEx and allocation reuse are outside coverage.',
                          'Release binary provides module and address evidence, not source symbols.',
                          'Formal working-set failures remain unresolved.'])
if (OUT/'pages.json').exists():
    pages=json.loads((OUT/'pages.json').read_text())
    baseline=min(pages,key=lambda r:abs(r['seconds']-stable[0]['seconds']))
    final=max(pages,key=lambda r:r['workingSet'])
    previous={(r['allocation'],r['kind']):r for r in baseline['regions']}
    growth=[]
    for region in final['regions']:
        old=previous.get((region['allocation'],region['kind']),{})
        increase=region['residentBytes']-old.get('residentBytes',0)
        if increase>0:growth.append(dict(**region,increaseBytes=increase,baselineResidentBytes=old.get('residentBytes',0)))
    summary['residentGrowth']=dict(baselineSeconds=baseline['seconds'],peakSeconds=final['seconds'],
                                  positiveRegions=sorted(growth,key=lambda r:r['increaseBytes'],reverse=True))
(OUT/'analysis.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8')
print(json.dumps({k:v for k,v in summary.items() if k!='byFirstNonSystemCaller'},indent=2))
print(json.dumps({k:{n:v for n,v in row.items() if n!='examples'} for k,row in stats.items()},indent=2))
