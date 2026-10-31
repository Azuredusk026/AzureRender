from pathlib import Path
import gzip,hashlib,json,shutil
root=Path.cwd();run=Path(__file__).resolve().parent
assert json.loads((run/'joint-validation-status.json').read_text())['status']=='passed'
out=root/'docs/acceptance/recovery/evidence/2026-10-31/full-scene-playable';out.mkdir(parents=True,exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def copy(p,name):
 data=p.read_bytes()
 if p.suffix in ['.json','.log','.py']:data=data.replace(b'\r\n',b'\n').rstrip()+b'\n'
 if name.endswith('.gz'):data=gzip.compress(data,mtime=0)
 (out/name).write_bytes(data)
for name in ['candidate.json','input-verification.json','collision-geometry.json','contact-geometry.json','configuration-compatibility.json','input-route.json','route-expectations.json','railing-input.json','railing-extended-input.json','railing-initial-review.json','joint-validation-status.json','runtime-cpu-debug.log','runtime-cpu-release.log']:
 copy(run/name,name)
for folder in ['stationary','moving-green','moving-debug','railing-green','railing-extended','fixed-regression']:
 for name in ['summary.json','runtime.json','player.log','behavior-verification.json','behavior-measurements.json','contact-verification.json']:
  p=run/folder/name
  if p.exists():copy(p,folder+'-'+name+('.gz' if name in ['runtime.json','player.log'] else ''))
 p=run/folder/'frames/capture_manifest.json'
 if p.exists():copy(p,folder+'-capture-manifest.json.gz')
for frame in [0,99,189,239,319,409,539]:
 copy(run/f'moving-green/frames/frame_{frame:06d}.png',f'route-frame-{frame:06d}.png')
copy(run/'railing-green/frames/frame_000239.png','initial-railing-settling.png')
copy(run/'railing-extended/frames/frame_000239.png','railing-contact.png')
copy(run/'railing-extended/frames/frame_000539.png','railing-settled.png')
copy(run/'fixed-regression/frames/frame_000002.png','fixed-full-scene.png')
for name in ['prepare.py','verify_inputs.py','measure_collision.py','contact_query.py','capture_route.py','review_route.py','verify_behavior.py','verify_railing.py','verify_railing_initial.py','capture_fixed.py','finish_validation.py','finish_joint_checks.py','archive_joint.py']:
 copy(run/name,name)
project=run/'project';inventory=[]
for p in sorted(project.rglob('*')):
 if p.is_file() and '.azure' not in p.relative_to(project).parts:
  inventory.append(dict(path=p.relative_to(project).as_posix(),bytes=p.stat().st_size,sha256=sha(p)))
blob=json.dumps(dict(schemaVersion=1,scope='Closed project inputs after initial asset import; cache excluded',files=inventory),indent=2).encode()+b'\n';(out/'project-inputs.json.gz').write_bytes(gzip.compress(blob,mtime=0))
release=json.loads((run/'moving-green/behavior-verification.json').read_text());debug=json.loads((run/'moving-debug/behavior-verification.json').read_text());railing=json.loads((run/'railing-extended/contact-verification.json').read_text());fixed=json.loads((run/'fixed-regression/summary.json').read_text())
summary=dict(status='independent-joint-validation-passed',sourceBase='316caf7',project='build/runs/'+run.name+'/project/playable.azureproject',projectSha256=sha(project/'playable.azureproject'),levelSha256=sha(project/'scene/playable.azurelevel'),projectInputFiles=len(inventory),unchangedMaterialFiles=26,characterModelSha256=sha(project/'content-character/hero-restored-inputs-v8.glb'),release=release,debug=debug,railing=railing,fixedRegressionFrames=len(fixed['frames']),formalQualityStage='Q1 executing; Q2/Q3/Q4 overall gates remain pending',physicalInput=False,publishing=False,scope='Component/script assembly, functional input replay, source collision geometry and byte-exact fixed visual regression; native appearance, native collision equivalence and performance remain separate.')
(out/'summary.json').write_bytes((json.dumps(summary,indent=2)+'\n').encode())
files=[dict(path=p.name,bytes=p.stat().st_size,sha256=sha(p)) for p in sorted(out.iterdir()) if p.name!='artifacts.json'];(out/'artifacts.json').write_bytes((json.dumps(dict(schemaVersion=1,files=files),indent=2)+'\n').encode());print(json.dumps(dict(status=summary['status'],files=len(files),bytes=sum(x['bytes'] for x in files),inputs=len(inventory))))
