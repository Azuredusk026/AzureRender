import gzip,hashlib,json,os,subprocess
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;dest=root/'docs/acceptance/recovery/evidence/2026-11-01/view-fog-resource-reuse';assert not dest.exists();dest.mkdir(parents=True)
assert json.loads((run/'private-gpu-summary.json').read_text())['status']=='passed';assert json.loads((run/'fixed-world-summary.json').read_text())['status']=='passed'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def copy(p,name,raw=False):
 q=dest/name;q.parent.mkdir(parents=True,exist_ok=True);data=p.read_bytes();q.write_bytes(gzip.compress(data,mtime=0) if raw else data.replace(b'\r\n',b'\n') if p.suffix in {'.py','.json','.md'} else data)
for p in sorted(run.iterdir()):
 if not p.is_file():continue
 if p.suffix in {'.json','.py','.png'}:
  copy(p,p.name)
  if p.suffix in {'.json','.py'}:copy(p,'raw/'+p.name+'.gz',True)
 elif p.suffix=='.log':copy(p,'logs/'+p.name+'.gz',True)
 elif p.suffix=='.gz':copy(p,'before/'+p.name)
for group in ['red-gpu','red-temporal-gpu','red-temporal-valid-gpu','public-gpu','public-gpu-green']:
 folder=run/group
 if not folder.exists():continue
 for directory,dirs,files in os.walk(folder):
  dirs[:]=[d for d in dirs if d!='.azure'];directory=Path(directory)
  for name in files:
   p=directory/name;relative=p.relative_to(run).as_posix()
   if p.suffix in {'.log','.jsonl'}:copy(p,relative.replace('/captures/','/diagnostics/')+'.gz',True)
   elif name in {'observations.json','summary.json'}:copy(p,relative)
   elif 'game' in p.relative_to(folder).parts and p.suffix in {'.azureproject','.azurelevel','.gltf','.azfog','.lua','.azmeta'}:copy(p,relative+'.gz',True)
for name in ['route-release','route-debug','fixed-world']:
 folder=run/name
 for item in ['runtime.json','player.log','captures/azureplayer.log.jsonl','frames/capture_manifest.json','summary.json']:
  p=folder/item
  if p.exists():copy(p,name+'/'+('diagnostics.jsonl.gz' if item.startswith('captures/') else item+'.gz'),True)
 selected=[0,100,190,299,360,410,451,539] if name.startswith('route') else [0,11]
 for frame in selected:
  filename='frame_'+str(frame).zfill(6)+'.png';copy(folder/'frames'/filename,name+'/frames/'+filename)
for name in ['playable','full-world-linear']:
 for p in [run/'project'/(name+'.azureproject'),run/'project/scene'/(name+'.azurelevel')]:copy(p,'inputs/'+p.relative_to(run/'project').as_posix()+'.gz',True)
changes=[]
for name in ['src/runtime/Level.cpp','src/scenes/CharacterSceneRenderer.cpp','tests/AssetLevelTests.cpp','tools/test_renderer_residency.py','tools/test_validation_protocol.py']:
 p=root/name;raw=p.read_bytes();canonical=raw.replace(b'\r\n',b'\n');q=dest/('source/'+name+'.gz');q.parent.mkdir(parents=True,exist_ok=True);q.write_bytes(gzip.compress(canonical,mtime=0));copy(p,'source-raw/'+name+'.gz',True);changes.append(dict(path=name,canonicalSha256=hashlib.sha256(canonical).hexdigest(),rawSha256=sha(p)))
reused=root/'docs/acceptance/nr/evidence/2026-11-01/joint-playable-display/manifest.json';manifest=dict(schemaVersion=1,visualAcceptance='failed-user-review',baseCommit='bc90025616c43b407bf94e5d6ec3b7ad7250bd0d',canonicalText='UTF-8 with LF. Captured logs, inputs and source snapshots retain exact raw bytes in gzip streams.',sourceChanges=changes,reusedEvidence=dict(path=reused.relative_to(root).as_posix(),sha256=sha(reused),scope='Frozen project inputs, source appearance controls and original route image fingerprints.'),files=[dict(path=p.relative_to(dest).as_posix(),bytes=p.stat().st_size,sha256=sha(p)) for p in sorted(dest.rglob('*')) if p.is_file()]);(dest/'manifest.json').write_bytes((json.dumps(manifest,indent=2)+'\n').encode());print('Archived',len(manifest['files']),'files')
