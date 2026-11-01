import gzip,hashlib,json,subprocess
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;dest=root/'docs/acceptance/nr/evidence/2026-11-01/joint-playable-display'
assert not dest.exists();dest.mkdir(parents=True)
assert json.loads((run/'review.json').read_text())['status']=='behavior-cross-configuration-passed'
assert json.loads((run/'native-effects-summary.json').read_text())['status']=='measured'
assert json.loads((run/'native-character-effects.json').read_text())['characterResponseBudgetPassed']
assert json.loads((run/'object-shadow-summary.json').read_text())['responseBudgetPassed']
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def copy(p,name,compressed=False):
 target=dest/name;target.parent.mkdir(parents=True,exist_ok=True);raw=p.read_bytes()
 target.write_bytes(gzip.compress(raw,mtime=0) if compressed else raw.replace(b'\r\n',b'\n') if p.suffix in {'.json','.py','.md'} else raw)
for p in sorted(run.iterdir()):
 if p.is_file() and p.suffix in {'.json','.py','.png'}:
  copy(p,p.name)
  if p.suffix in {'.json','.py'}:copy(p,'raw/'+p.name+'.gz',True)
  if p.name=='ctest-inventory.json':(dest/p.name).write_bytes((json.dumps(json.loads(p.read_text()),indent=2)+'\n').encode())
 elif p.is_file() and p.suffix=='.log':copy(p,'logs/'+p.name+'.gz',True)
for case in ['project-check','route-release','route-debug','native-entry']:
 folder=run/case
 for name in ['summary.json','behavior-verification.json','frame-fingerprints.json']:
  p=folder/name
  if p.exists():copy(p,case+'/'+name)
 for name in ['runtime.json','player.log','captures/azureplayer.log.jsonl','frames/capture_manifest.json']:
  p=folder/name
  if p.exists():copy(p,case+'/'+('diagnostics.jsonl.gz' if name.startswith('captures/') else name+'.gz'),True)
 if case.startswith('route-'):
  for frame in [0,100,190,299,360,410,451,539]:
   name='frame_'+str(frame).zfill(6)+'.png';copy(folder/'frames'/name,case+'/frames/'+name)
 if case=='native-entry':
  for p in (folder/'frames').glob('frame_*.png'):copy(p,case+'/frames/'+p.name)
for folder in sorted((run/'native-effects').iterdir()):
 if not folder.is_dir():continue
 for name in ['runtime.json','player.log','captures/azureplayer.log.jsonl','frames/capture_manifest.json']:
  copy(folder/name,'native-effects/'+folder.name+'/'+('diagnostics.jsonl.gz' if name.startswith('captures/') else name+'.gz'),True)
 copy(folder/'frames/frame_000000.png','native-effects/'+folder.name+'/frame_000000.png')
for group in ['standalone-captures','object-shadow-captures']:
 for folder in sorted((run/group).iterdir()):
  if not folder.is_dir():continue
  for name in ['runtime.json','player.log','captures/azureplayer.log.jsonl','frames/capture_manifest.json']:
   copy(folder/name,group+'/'+folder.name+'/'+('diagnostics.jsonl.gz' if name.startswith('captures/') else name+'.gz'),True)
  copy(folder/'frames/frame_000000.png',group+'/'+folder.name+'/frame_000000.png')
for name in ['playable','near-world-linear','near-linear-isolated']:
 for p in [run/'project'/(name+'.azureproject'),run/'project/scene'/(name+'.azurelevel')]:copy(p,'inputs/'+p.relative_to(run/'project').as_posix()+'.gz',True)
for p in (run/'project/assets/linear-display-materials').iterdir():
 if p.is_file():copy(p,'inputs/assets/linear-display-materials/'+p.name+'.gz',True)
for name in ['geometry-only','source-standalone','object-shadow-disabled-world','object-shadow-disabled-source']:
 for p in [run/'standalone-project'/(name+'.azureproject'),run/'standalone-project/scene'/(name+'.azurelevel')]:copy(p,'standalone-inputs/'+p.relative_to(run/'standalone-project').as_posix()+'.gz',True)
diagnostics=json.loads((run/'configuration-image-diagnostics.json').read_text());selected={row['frame'] for key in ['changedPixels','maximumLinearError'] for row in [max(diagnostics['rows'],key=lambda r:r[key])]}|{min(diagnostics['rows'],key=lambda r:r['srgbLuminanceSsim'])['frame']}
for cfg in ['release','debug']:
 for frame in sorted(selected):
  name='frame_'+str(frame).zfill(6)+'.png';copy(run/('route-'+cfg)/'frames'/name,'route-'+cfg+'/frames/'+name)
reused=root/'docs/acceptance/nr/evidence/2026-11-01/material-display-transform/manifest.json'
manifest=dict(schemaVersion=1,visualAcceptance='failed-user-review',baseCommit=json.loads((run/'candidate.json').read_text())['baseCommit'],capturedAtCommit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),canonicalText='UTF-8 with LF. Raw logs, runtime reports and project input payloads retain exact bytes in gzip streams.',reusedEvidence=dict(path=reused.relative_to(root).as_posix(),sha256=sha(reused),scope='Unchanged compiled source, programs, shader package, public display contracts and frozen native interior comparisons.'),files=[dict(path=p.relative_to(dest).as_posix(),bytes=p.stat().st_size,sha256=sha(p)) for p in sorted(dest.rglob('*')) if p.is_file()])
(dest/'manifest.json').write_bytes((json.dumps(manifest,indent=2)+'\n').encode());print('Archived',len(manifest['files']),'files')
