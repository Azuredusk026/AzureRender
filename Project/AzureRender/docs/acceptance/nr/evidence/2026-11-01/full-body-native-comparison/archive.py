import gzip,hashlib,json,os,subprocess
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;dest=root/'docs/acceptance/nr/evidence/2026-11-01/full-body-native-comparison';assert not dest.exists();dest.mkdir(parents=True);sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for name in ['capture-summary.json','diagnostic-capture-summary.json','mask-capture-summary.json']:assert json.loads((run/name).read_text())['status']=='captured-inputs-unchanged'
def copy(p,name,raw=False):
 q=dest/name;q.parent.mkdir(parents=True,exist_ok=True);data=p.read_bytes();q.write_bytes(gzip.compress(data,mtime=0) if raw else data.replace(b'\r\n',b'\n') if p.suffix in {'.py','.json','.md'} else data)
for p in sorted(run.iterdir()):
 if p.suffix in {'.py','.json','.png'} and p.is_file():
  copy(p,p.name)
  if p.suffix in {'.py','.json'}:copy(p,'raw/'+p.name+'.gz',True)
 elif p.suffix=='.log':copy(p,'logs/'+p.name+'.gz',True)
for group in ['captures','diagnostic-captures','mask-captures','failed-relative-diagnostic-captures']:
 for folder in (run/group).iterdir():
  if not folder.is_dir():continue
  for name in ['player.log','runtime.json','captures/azureplayer.log.jsonl','frames/capture_manifest.json']:
   p=folder/name
   if p.exists():copy(p,group.replace('captures','runs')+'/'+folder.name+'/'+name.replace('captures/','diagnostics/')+'.gz',True)
  for frame in ([0,31,63] if group in ['captures','mask-captures'] else [0]):
   p=folder/'frames'/f'frame_{frame:06d}.png'
   if p.exists():copy(p,group.replace('captures','runs')+'/'+folder.name+'/frames/'+p.name)
for folder_name in ['project/scene','diagnostic-project-v2/scene','diagnostic-project/scene','mask-project/assets']:
 folder=run/folder_name
 for p in folder.glob('*.azurelevel'):
  if folder_name=='project/scene' and not p.name.startswith('fullbody-'):continue
  if folder_name=='diagnostic-project-v2/scene' and not p.name.startswith('fullbody-no-'):continue
  copy(p,'inputs/'+folder_name+'/'+p.name+'.gz',True)
for folder_name in ['project','diagnostic-project-v2','diagnostic-project','mask-project']:
 for p in (run/folder_name).glob('*.azureproject'):
  if folder_name=='project' and not p.name.startswith('fullbody-'):continue
  if folder_name=='diagnostic-project-v2' and not p.name.startswith('fullbody-no-'):continue
  copy(p,'inputs/'+folder_name+'/'+p.name+'.gz',True)
reused=['docs/acceptance/recovery/evidence/2026-11-01/view-fog-resource-reuse/manifest.json','docs/acceptance/recovery/evidence/2026-11-01/view-fog-resource-reuse/release-build.json','docs/acceptance/nr/evidence/2026-11-01/joint-playable-display/manifest.json'];m=dict(schemaVersion=1,baseCommit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),visualAcceptance='failed-user-review',referenceQualification='Engine controls; native full-frame qualification remains pending.',reusedEvidence=[dict(path=p,sha256=sha(root/p)) for p in reused],files=[dict(path=p.relative_to(dest).as_posix(),bytes=p.stat().st_size,sha256=sha(p)) for p in sorted(dest.rglob('*')) if p.is_file()]);(dest/'manifest.json').write_bytes((json.dumps(m,indent=2)+'\n').encode());print('Archived',len(m['files']),'files')
