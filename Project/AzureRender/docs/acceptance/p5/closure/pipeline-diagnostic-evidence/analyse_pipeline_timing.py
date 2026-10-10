import gzip,hashlib,json,re
from pathlib import Path
root=Path.cwd().resolve();old=root/'build/p5-p6-closure-2026-10-10';out=root/'build/p5-p6-inspection-2026-10-11'
rows=[]
for name in ('inspection-timed-native-trace','disabled-timed-native-trace'):
    folder=old/name;summary=json.loads((folder/'summary.json').read_text())
    assert summary['childExitCode']==0 and all(summary[k]==0 for k in ('dropped','reentrant','invalidArguments','unpublished'))
    events=json.loads(gzip.decompress((folder/'events.json.gz').read_bytes()))
    stamps=re.findall(r'AZURE_DIAGNOSTIC_PIPELINE (graphics|compute) begin=(\d+) end=(\d+) count=(\d+)',(folder/'stderr.log').read_text())
    assert len(stamps)==21 and sum(int(x[3]) for x in stamps)==21
    origin=summary['startedCounter'];frequency=summary['counterFrequency']
    completed=max(int(x[2])/frequency-origin for x in stamps)
    commits=[]
    for event in events:
        if event['api'] not in (1,2) or event['status']&0x80000000 or not event['flags']&0x1000 or event['size']<=1024**2:continue
        modules=sorted({Path(module['path']).name for address in event['stack'] for module in summary['modules']
                        if module['base']<=address<module['base']+module['size']})
        commits.append(dict(seconds=event['qpc']/frequency-origin,thread=event['threadId'],bytes=event['size'],base=event['base'],modules=modules))
    late=[event for event in commits if event['seconds']>completed]
    driver_late=[event for event in late if any('nv' in name.lower() for name in event['modules'])]
    rows.append(dict(case=name,pipelineCreates=21,graphics=sum(x[0]=='graphics' for x in stamps),compute=sum(x[0]=='compute' for x in stamps),
                     pipelineCompletionSeconds=completed,largeCommits=commits,lateLargeCommits=late,lateDriverCommits=driver_late,
                     diagnosticOnly=True,formalAcceptance=False,sourceHashes=summary['hashes'],
                     evidence={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in folder.iterdir() if p.is_file()}))
assert not rows[0]['lateLargeCommits'] and rows[1]['lateDriverCommits']
result=dict(schemaVersion=1,causalScope='Owned process, current NVIDIA driver, instrumented timing contrast; public API does not guarantee cross-driver completion',
            diagnosticOnly=True,productionPerformanceVerified=False,checks=rows)
(out/'pipeline-timing-analysis.json').write_text(json.dumps(result,indent=2)+'\n')
for row in rows:print(row['case'],'created by',round(row['pipelineCompletionSeconds'],3),'late commits',[(round(x['seconds'],3),x['bytes']) for x in row['lateDriverCommits']])
