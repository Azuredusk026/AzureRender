from pathlib import Path
import json,subprocess,sys,math
r=Path(__file__).resolve().parent
checks=[['verify_behavior.py','--name','moving-green'],['verify_behavior.py','--name','moving-debug'],['verify_railing.py'],['verify_inputs.py']]
for i,step in enumerate(checks):
 result=subprocess.run([sys.executable,'-X','utf8',str(r/step[0]),*step[1:]],capture_output=True)
 (r/f'final-check-{i}.log').write_bytes(result.stdout+result.stderr);assert result.returncode==0,step
fixed=json.loads((r/'fixed-regression/summary.json').read_text());assert fixed['status']=='passed' and len(fixed['frames'])==3 and all(f['changedPixels']==0 for f in fixed['frames'])
d=json.loads((r/'stationary/runtime.json').read_text());rows=d['routeFrames'];heroes=[next(c for c in f['characters'] if c['node']=='hero') for f in rows];floor=json.loads((r/'route-expectations.json').read_text())['groundHeightAtSpawn']
assert len(rows)==90 and all(h['state']=='idle' for h in heroes)
assert all(h['grounded'] for h in heroes[-60:])
assert all(abs(h['position'][1]-floor)<.0001 and h['position'][0]==0 and h['position'][2]==9 for h in heroes[-60:])
report=dict(status='passed',frames=90,lastSixtyGroundedIdle=True,referenceGroundHeight=floor,maximumSettledHeightError=max(abs(h['position'][1]-floor) for h in heroes[-60:]),scope='Independent source triangle height and production capsule contact at spawn; not a performance or native collider equivalence result')
(r/'stationary/contact-verification.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
(r/'joint-validation-status.json').write_bytes((json.dumps(dict(status='passed',checks=checks,fixedRegressionFrames=3,stationaryFrames=90,initialRailingSettlingFailureRetained='railing-initial-review.json',extendedRailingFrames=540),indent=2)+'\n').encode());print('Joint functional checks, stationary contact and fixed pixel regression passed')
