from pathlib import Path
import json,math
r=Path(__file__).resolve().parent;out=r/'railing-extended';d=json.loads((out/'runtime.json').read_text());rows=d['routeFrames'];heroes=[next(c for c in f['characters'] if c['node']=='hero') for f in rows];positions=[h['position'] for h in heroes]
expected=json.loads((r/'route-expectations.json').read_text());plane=expected['forwardRailingRayZ'];radius=.3;floor=expected['groundHeightAtSpawn']
assert len(rows)==540 and d['replayedActions']==5
assert all(not f['loadError'] and not f['scriptErrors'] and not f['presentationErrors'] for f in rows)
assert min(p[2] for p in positions)>=plane+radius-.01,'Capsule crossed the independently queried barrier plane'
held=[i for i,f in enumerate(rows) if 150<=f['frame']<=280]
assert all(plane+radius-.01<=positions[i][2]<=plane+radius+.1 for i in held),'Continued forward input did not reach the source barrier'
assert min(p[1] for p in positions)>floor-.01
assert all(h['grounded'] and h['state']=='idle' for h in heroes[-60:])
speed=[math.hypot(b[0]-a[0],b[2]-a[2])*60 for a,b in zip(positions,positions[1:])]
assert all(speed[i-1]<.0001 and heroes[i]['state']=='idle' for i in held),'Continued input was not blocked'
assert max(speed[-60:])<.0001,'Final contact is not at rest'
assert sorted(set(f['levelRevision'] for f in rows))==[2]
report=dict(status='passed',frames=540,actions=5,barrierRayPlaneZ=plane,capsuleRadius=radius,heldFrames=[150,280],heldContactPosition=positions[held[0]],heldMaximumHorizontalSpeed=max(speed[i-1] for i in held),finalPosition=positions[-1],minimumZ=min(p[2] for p in positions),lastSixtyGroundedIdle=True,lastSixtyMaximumHorizontalSpeed=max(speed[-60:]),revision=2,scope='Continued-input blocking against the declared railing, then release and settling; native collider equivalence remains separate.')
(out/'contact-verification.json').write_bytes((json.dumps(report,indent=2)+'\n').encode());print(json.dumps(report,indent=2))
