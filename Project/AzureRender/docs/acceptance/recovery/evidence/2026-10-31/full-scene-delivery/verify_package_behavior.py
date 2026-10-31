from pathlib import Path
import argparse,json,math,statistics
r=Path(__file__).resolve().parent
p=argparse.ArgumentParser();p.add_argument('--name',required=True);args=p.parse_args();out=r/args.name
d=json.loads((out/'runtime.json').read_text());rows=d['routeFrames'];by_frame={f['frame']:f for f in rows}
hero=lambda f:next(c for c in f['characters'] if c['node']=='hero')
position=lambda f:hero(f)['position']
sample=lambda f,clip:sum(s['weight'] for a in f['animations'] if a['node']=='hero' for s in a['samples'] if s['clip']==clip)
speed={f['frame']:math.hypot(position(f)[0]-position(rows[i-1])[0],position(f)[2]-position(rows[i-1])[2])*60 for i,f in enumerate(rows) if i}
assert len(rows)==540 and d['replayedActions']==13
assert not d['scriptErrors'] and not d['presentationErrors']
assert all(not f['scriptErrors'] and not f['presentationErrors'] and not f['loadError'] for f in rows)
walk=statistics.median(speed[f] for f in range(60,111));run=statistics.median(speed[f] for f in range(150,196))
assert abs(walk-1.2)<.0001 and abs(run-3)<.002,(walk,run)
assert all(sample(by_frame[f],7)>.999 for f in range(100,111))
assert all(sample(by_frame[f],8)>.999 for f in range(185,196))
for first,last,focused in [(305,328,False),(335,358,True)]:
 for i in range(first,last+1):
  f=by_frame[i];h=hero(f);assert f['focused']==focused and h['state']=='idle'
  assert math.hypot(h['velocity'][0],h['velocity'][2])<1e-7,'Held input survived focus reset'
  assert speed[i]<.1,'Actual motion exceeds the configured idle threshold'
transitions=[(f['frame'],rows[i-1]['levelRevision'],f['levelRevision']) for i,f in enumerate(rows) if i and f['levelRevision']!=rows[i-1]['levelRevision']]
assert transitions==[(451,2,3)],transitions
assert all(hero(by_frame[i])['grounded'] and hero(by_frame[i])['state']=='idle' for i in range(510,540))
assert all(math.dist(position(by_frame[i]),[0,1.0324134826660156,9])<.0001 for i in range(510,540))
assert all(f['levelReference']=='stage:/playable.azurelevel' for f in rows)
obstructed=sum(f['cameraObstructed'] for f in rows);assert obstructed>0
distances=[math.dist(f['camera'],f['target']) for f in rows]
assert min(distances)>.04 and max(distances)<4.02
# The public controller uses yaw; the lens-to-target vector also has shoulder offset.
f=by_frame[410];before=by_frame[400];delta=[position(f)[i]-position(before)[i] for i in [0,2]];forward=[f['target'][i]-f['camera'][i] for i in [0,2]]
raw_view_cosine=sum(a*b for a,b in zip(delta,forward))/(math.hypot(*delta)*math.hypot(*forward))
yaw=math.radians(-450*.15);expected=[math.sin(yaw),-math.cos(yaw)]
cosine=sum(a*b for a,b in zip(delta,expected))/math.hypot(*delta);assert cosine>.999999,cosine
report=dict(status='passed',frames=540,actions=13,walkSpeed=walk,sprintSpeed=run,walkClip=7,sprintClip=8,focusReleased=True,revisionTransitions=transitions,finalIdleGrounded=True,obstructedFrames=obstructed,cameraDistanceRange=[min(distances),max(distances)],cameraRelativeForwardCosine=cosine,rawLensForwardCosine=raw_view_cosine,cameraYawDegrees=-67.5,cameraSensitivitySource='src/ecs/GameComponents.hpp: default .15 degrees per input unit',minimumCharacterHeight=min(position(f)[1] for f in rows),scope='Production fixed-step input, actual displacement, animation samples, focus, camera collision and script restart. Planter traversal is not a wall-blocking or native-collision-equivalence result.')
(out/'behavior-verification.json').write_bytes((json.dumps(report,indent=2)+'\n').encode());print(json.dumps(report,indent=2))
