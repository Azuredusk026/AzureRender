import hashlib,json
from pathlib import Path
run=Path(__file__).resolve().parent;sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();rows=[]
for case in ['fullbody-world-aa0','fullbody-source-aa0']:
    before=run/'before-field/native-captures'/case/'frames';after=run/'native-captures'/case/'frames'
    old={p.name:sha(p) for p in before.glob('frame_*.png')};new={p.name:sha(p) for p in after.glob('frame_*.png')};assert len(old)==len(new)==64
    different=[name for name in old if old[name]!=new[name]];rows.append(dict(name=case,frames=64,differentFrames=different))
report=dict(status='passed' if all(not r['differentFrames'] for r in rows) else 'failed',scope='The scene probe adaptation fix preserves the existing native material policy candidate outside the mixed-source field control.',cases=rows)
(run/'revision-comparison.json').write_bytes((json.dumps(report,indent=2)+'\n').encode());print(json.dumps(report));assert report['status']=='passed'
