from pathlib import Path
import json,math
r=Path(__file__).resolve().parent;out=r/'railing-green';d=json.loads((out/'runtime.json').read_text());rows=d['routeFrames'];heroes=[next(c for c in f['characters'] if c['node']=='hero') for f in rows];positions=[h['position'] for h in heroes]
expected=json.loads((r/'route-expectations.json').read_text());plane=expected['forwardRailingRayZ'];radius=.3;floor=expected['groundHeightAtSpawn']
assert len(rows)==240 and d['replayedActions']==5
assert all(not f['loadError'] and not f['scriptErrors'] and not f['presentationErrors'] for f in rows)
assert min(p[2] for p in positions)>=plane+radius-.01,'Capsule crossed the independently queried barrier plane'
assert plane+radius-.01<=positions[-1][2]<=plane+radius+.1,'Route did not reach the source barrier'
assert min(p[1] for p in positions)>floor-.01
assert all(h['grounded'] and h['state']=='idle' for h in heroes[-60:])
speed=[math.hypot(b[0]-a[0],b[2]-a[2])*60 for a,b in zip(positions,positions[1:])]
assert max(speed[-60:])<.0001,'Final contact is not at rest'
assert sorted(set(f['levelRevision'] for f in rows))==[2]
report=dict(status='passed',frames=240,actions=5,barrierRayPlaneZ=plane,capsuleRadius=radius,finalPosition=positions[-1],minimumZ=min(p[2] for p in positions),lastSixtyGroundedIdle=True,lastSixtyMaximumHorizontalSpeed=max(speed[-60:]),revision=2,scope='Blocking against declared source railing geometry; native collider equivalence remains separate.')
(out/'contact-verification.json').write_bytes((json.dumps(report,indent=2)+'\n').encode());print(json.dumps(report,indent=2))
