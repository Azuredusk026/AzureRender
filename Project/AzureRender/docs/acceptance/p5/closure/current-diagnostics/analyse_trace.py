from pathlib import Path
import json,collections
root=Path('build/p5-p6-closure-2026-10-10')
events=[json.loads(s) for s in (root/'editor-return-trace/events.jsonl').read_text().splitlines()]
rows=json.loads((root/'trace-residency.json').read_text())['snapshots']
def module(e):
    frames=[f for f in e['stack'] if not any(x in f['module'].lower() for x in ('ntdll.dll','kernelbase.dll','kernel32.dll'))]
    return frames[0]['module'] if frames else 'unknown'
groups=collections.defaultdict(lambda:dict(calls=0,requestedBytes=0,releaseCalls=0,functions=collections.Counter()))
for e in events:
    g=groups[module(e)];g['calls']+=1;g['requestedBytes']+=e['size'] if e['action']=='allocate' else 0
    g['releaseCalls']+=e['action']=='release'
    for f in e['stack']:
        if f['module']==module(e):g['functions'][f['function']]+=1;break
for g in groups.values():g['functions']=dict(g['functions'].most_common(5))
peak=max(rows,key=lambda r:r['counters']['working'])
first=rows[len(rows)//2]
def correlate(s):
    result=[]
    for r in s['regions']:
        matching=[e for e in events if e['action']=='allocate' and not e['status']&0x80000000 and e['base']==r['allocation']]
        result.append(dict(**r,modules=dict(collections.Counter(module(e) for e in matching)),calls=len(matching)))
    return sorted(result,key=lambda r:r['residentBytes'],reverse=True)
result=dict(diagnosticOnly=True,entryAndReturnGroups=groups,snapshotCount=len(rows),
    firstSeconds=first['seconds'],peakSeconds=peak['seconds'],firstCounters=first['counters'],peakCounters=peak['counters'],
    peakRegions=correlate(peak),firstRegions=correlate(first),
    limits=['Observer attached after process launch; debugger timing cannot establish formal performance.','Private heap region ownership is partial; mapped and image regions have separate paths.'])
(root/'trace-analysis.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:result[k] for k in ('snapshotCount','firstCounters','peakCounters')}))
print(json.dumps(groups,indent=2)[:6500])
