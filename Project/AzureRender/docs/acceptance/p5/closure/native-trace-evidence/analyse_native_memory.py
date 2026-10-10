from pathlib import Path
import gzip,json,hashlib
root=Path.cwd();w=root/'build/p5-p6-closure-2026-10-10';trace=w/'native-exploration-prestart-tls-trace'
s=json.loads((trace/'summary.json').read_text());events=json.loads(gzip.decompress((trace/'events.json.gz').read_bytes()))
memory=json.loads((trace/'residency.json').read_text());modules=s['modules']
def stack(event):
    rows=[]
    for address in event['stack']:
        candidates=[m for m in modules if m['base']<=address<m['base']+m['size']]
        rows.append(dict(address=address,candidates=[dict(module=m['path'],offset=address-m['base']) for m in candidates]))
    return rows
targets=[]
for index,event in enumerate(events):
    if event['api'] not in (1,2) or event['status']&0x80000000 or not event['flags']&0x1000 or not 14000000<event['size']<16000000:continue
    resolved=stack(event)
    if not any('nvoglv64.dll' in candidate['module'].lower() for frame in resolved for candidate in frame['candidates']):continue
    lo,hi=event['base'],event['base']+event['size']
    related=[dict(index=i,**e) for i,e in enumerate(events) if (lo<=e['base']<hi or lo<=e['inputBase']<hi)]
    resident=[]
    for snapshot in memory['snapshots']:
        row=next((r for r in snapshot['regions'] if r['allocation']==lo),None)
        resident.append(dict(counterSeconds=snapshot['counter'],residentBytes=row['residentBytes'] if row else 0,
                             racedPages=snapshot['racedPages']))
    targets.append(dict(index=index,event=event,stack=resolved,overlappingCalls=related,residentTimeline=resident))
assert len(targets)==1 and not any(s[k] for k in ('dropped','reentrant','invalidArguments','unpublished'))
result=dict(diagnosticOnly=True,sourceCommit=__import__('subprocess').run(['git','rev-parse','HEAD'],capture_output=True,text=True,check=True).stdout.strip(),
    completeAllocationCoverage=False,rootCauseConfirmedForFormalFailures=False,targets=targets,
    loadedCaptureModules=[m for m in modules if 'graphics-hook' in m['path'].lower()],
    hashes={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (trace/'summary.json',trace/'events.bin',trace/'residency.json')},
    limits=['Allocation and residency are correlated within this instrumented process only.',
            'Formal performance failures have different run sources and require independent causal verification.',
            'Module snapshots and 16-frame stacks do not identify heap suballocation ownership.'])
(w/'native-memory-analysis.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(dict(events=s['events'],targets=len(targets),committedBytes=targets[0]['event']['size'],residentBytes=max(x['residentBytes'] for x in targets[0]['residentTimeline']),captureModules=len(result['loadedCaptureModules']))))
