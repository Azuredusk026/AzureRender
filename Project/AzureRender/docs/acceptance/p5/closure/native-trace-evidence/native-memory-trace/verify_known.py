from pathlib import Path
import argparse,gzip,json
p=argparse.ArgumentParser();p.add_argument('trace',type=Path);p.add_argument('proof',type=Path);a=p.parse_args()
events=json.loads(gzip.decompress((a.trace/'events.json.gz').read_bytes()))
proof=json.loads(a.proof.read_text());used=set();rows=[]
for thread in proof:
 cursor=-1
 for expected in thread['proof']['events']:
  candidates=[]
  for index,event in enumerate(events):
   if index<=cursor or index in used or event['threadId']!=thread['threadId']:continue
   base=event['inputBase'] if expected['action']=='release' else event['base']
   if (event['api']==(3 if expected['action']=='release' else 1) and event['flags']==expected['flags']
       and event['status']==expected['status'] and base==expected['base'] and event['inputSize']==expected['size']):candidates.append(index)
  found=candidates[0] if candidates else None
  if found is not None:used.add(found);cursor=found
  rows.append(dict(threadId=thread['threadId'],expected=expected,eventIndex=found))
summary=json.loads((a.trace/'summary.json').read_text())
result=dict(expectedCalls=len(rows),matchedCalls=len(used),orderedWithinThread=True,
 passed=len(used)==len(rows) and not any(summary[k] for k in ('dropped','reentrant','invalidArguments','unpublished')),details=rows)
(a.trace/'known-calls-ordered.json').write_text(json.dumps(result,indent=2)+'\n')
print({k:v for k,v in result.items() if k!='details'})
assert result['passed']
