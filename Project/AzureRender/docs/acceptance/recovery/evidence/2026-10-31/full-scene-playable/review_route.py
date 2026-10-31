from pathlib import Path
import argparse,json,math,statistics
r=Path(__file__).resolve().parent
parser=argparse.ArgumentParser();parser.add_argument('--name',default='moving-green');args=parser.parse_args()
d=json.loads((r/args.name/'runtime.json').read_text());rows=d['routeFrames']
heroes=[next(c for c in row['characters'] if c['node']=='hero') for row in rows]
positions=[h['position'] for h in heroes]
speed=[0.0]+[math.hypot(b[0]-a[0],b[2]-a[2])*60 for a,b in zip(positions,positions[1:])]
windows={}
for name,first,last in [('idle',5,28),('walk',60,110),('run',150,195),('planter-traversal',230,249),('stop',255,269),('left',280,298),('unfocused',305,328),('refocused',335,358),('orbit',365,388),('forward',400,418),('restarted',510,539)]:
 a=next(i for i,row in enumerate(rows) if row['frame']==first);b=next(i for i,row in enumerate(rows) if row['frame']==last)
 segment=rows[a:b+1];hs=heroes[a:b+1]
 samples={}
 for row in segment:
  anim=next(x for x in row['animations'] if x['node']=='hero')
  for sample in anim['samples']:samples.setdefault(str(sample['clip']),[]).append(sample['weight'])
 windows[name]=dict(frames=[first,last],firstPosition=positions[a],lastPosition=positions[b],medianHorizontalSpeed=statistics.median(speed[a:b+1]),maximumHorizontalSpeed=max(speed[a:b+1]),groundedFrames=sum(h['grounded'] for h in hs),states=sorted(set(h['state'] for h in hs)),sampleWeightRanges={k:[min(v),max(v)] for k,v in samples.items()},focused=sorted(set(row['focused'] for row in segment)),cameraObstructedFrames=sum(row['cameraObstructed'] for row in segment))
snapshots=[]
for i in [0,29,60,119,150,195,225,239,249,269,299,300,329,330,359,360,389,419,449,450,451,479,510,539]:
 snapshots.append(dict(frame=rows[i]['frame'],rowIndex=i,position=positions[i],speed=speed[i],state=heroes[i]['state'],samples=next(x for x in rows[i]['animations'] if x['node']=='hero')['samples'],camera=rows[i]['camera'],target=rows[i]['target'],levelRevision=rows[i]['levelRevision'],levelReference=rows[i]['levelReference'],loading=rows[i]['loading']))
report=dict(status='measured-review-pending',frames=len(rows),windows=windows,levelRevisions=sorted(set(row['levelRevision'] for row in rows)),revisionTransitions=[dict(frame=row['frame'],rowIndex=i,before=rows[i-1]['levelRevision'],after=row['levelRevision']) for i,row in enumerate(rows) if i and row['levelRevision']!=rows[i-1]['levelRevision']],minimumHeight=min(p[1] for p in positions),maximumX=max(p[0] for p in positions),samples=snapshots,scope='Speeds derive from observed position differences at fixed 60 Hz; physics integrator velocity is not displacement. Observation frame is the completed-frame counter; input frame is the pre-step counter.')
(r/args.name/'behavior-measurements.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
print(json.dumps({k:v for k,v in report.items() if k not in ['samples']},indent=2))
