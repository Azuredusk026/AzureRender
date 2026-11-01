import gzip,hashlib,json,subprocess
from pathlib import Path
run=Path(__file__).resolve().parent;root=run.parents[2];dest=root/'docs/acceptance/nr/evidence/2026-11-01/native-cloth-inputs'
if dest.exists():
 previous=json.loads((dest/'manifest.json').read_text())
 for row in previous['files']:assert hashlib.sha256((dest/row['path']).read_bytes()).hexdigest()==row['sha256']
dest.mkdir(parents=True,exist_ok=True);files=[]
def add(path,name=None,compress=False):
 data=path.read_bytes();name=name or path.relative_to(run).as_posix();name+= '.gz' if compress else '';target=dest/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(gzip.compress(data,mtime=0) if compress else data)
 files.append(dict(path=name,sha256=hashlib.sha256(target.read_bytes()).hexdigest(),bytes=target.stat().st_size,source=dict(path=path.relative_to(root).as_posix(),sha256=hashlib.sha256(data).hexdigest(),bytes=len(data)),gzip=compress))
names=['source-manifest.json','authored-texture-sources.json','shared-specular-ramp-verification.json','native-bindings.json','variant-targets.json','variant-traces.json','variant-pixel-selection.json','native-binding-qualified.json','variant-specular-proof.json','transparent-bindings.json','transparent-texture-identity.json','native-npr-constant-rows.json','profile-lighting-differences.json','scene-npr-source-controls.json','asset-image-index.json','native-variant-contact.png']
for name in names:add(run/name)
for path in sorted(run.glob('*.py')):add(path,'tools/'+path.name)
for path in sorted((run/'source').glob('*.gz')):add(path)
for path in sorted(run.glob('variant-trace-*.json')):add(path,compress=True)
for base in ['native-bindings','variant-targets','transparent-bindings']:
 for path in sorted((run/base).glob('e*/*')):
  if path.name.endswith(('.bin','.asm','.dxbc')) and not path.name.startswith('target-'):add(path,compress=True)
add(run/'native-bindings/textures/ResourceId-6177.dds',compress=True)
for slot in [15,19,20]:
 draw=json.loads((run/'transparent-bindings.json').read_text())['draws'][0];entry=next(v for v in draw['textures'] if v['slot']==slot);add(run/'transparent-bindings'/entry['export']['path'],compress=True)
add(run/'native-binding-measurement.json','diagnostics/unqualified-binding-measurement.json')
author=root/'build/runs/20261017-000956-endfield-native-3caf73/laevat-static'
for name in ['source_audit.json','source_texture_artifacts.json']:add(author/name,'author/'+name,compress=True)
artifacts=json.loads((author/'source_texture_artifacts.json').read_text());identity=json.loads((run/'transparent-texture-identity.json').read_text())
for row in identity['textures']:
 source=row['authorIdentity'];artifact=next(a for a in artifacts if a['object']['source']==source['source'] and a['object']['path_id']==source['path_id']);add(author/artifact['png']['path'],'author/'+source['name']+'.png')
active=root/'build/runs/20261101-120125-character-environment-reception-fa70a7/candidate-project'
for index in [1,2,3,4,5]:add(active/f'assets/linear-display-materials/material-{index}.azurematerial',f'current/material-{index}.azurematerial',compress=True)
add(active/'scene/fullbody-world-aa0.azurelevel','current/fullbody-world-aa0.azurelevel',compress=True)
capture=json.loads((run/'native-binding-qualified.json').read_text());manifest=dict(schemaVersion=1,status='qualified-source-inputs',scope='Native clothing resource bindings and two debugger specular reconstructions; complete appearance remains failed.',sourceCommit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),sourceRun=run.relative_to(root).as_posix(),capture=dict(path=capture['capture'],sha256=capture['captureSha256']),files=files,visualAcceptance='failed-user-review')
(dest/'manifest.json').write_bytes((json.dumps(manifest,ensure_ascii=False,indent=2)+'\n').encode());print(json.dumps(dict(files=len(files),bytes=sum(v['bytes'] for v in files),status=manifest['status'])))
