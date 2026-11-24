import gzip,hashlib,json,re,subprocess
from pathlib import Path
run=Path(__file__).resolve().parent;root=run.parents[2];dest=root/'docs/acceptance/nr/evidence/2026-11-01/native-cloth-inputs';manifest=json.loads((dest/'manifest.json').read_text());sha=lambda b:hashlib.sha256(b).hexdigest()
for row in manifest['files']:
 data=(dest/row['path']).read_bytes();assert len(data)==row['bytes'] and sha(data)==row['sha256'],row['path']
 raw=gzip.decompress(data) if row['gzip'] else data;assert len(raw)==row['source']['bytes'] and sha(raw)==row['source']['sha256'],row['path']
 assert sha((root/row['source']['path']).read_bytes())==row['source']['sha256'],row['source']['path']
bindings=json.loads((run/'native-binding-qualified.json').read_text());assert bindings['status']=='qualified-shared-specular-ramp' and len(bindings['draws'])==5
assert [v['specularRampSlot'] for v in bindings['draws']]==[12,12,12,12,15]
proof=json.loads((run/'variant-specular-proof.json').read_text());assert proof['status']=='verified-variant-specular-intermediates' and len(proof['draws'])==2
identity=json.loads((run/'transparent-texture-identity.json').read_text());assert identity['status']=='qualified-transparent-author-textures' and all(v['flipYMaximumByteError']==0 for v in identity['textures'])
traces=json.loads((run/'variant-traces.json').read_text());assert traces['status']=='exported-variant-traces' and len(traces['traces'])==4
for row in traces['traces']:
 p=run/row['path'];assert sha(p.read_bytes())==row['sha256'];assert json.loads(p.read_text())['complete']
doc=root/'docs/acceptance/nr/native-cloth-inputs.md';text=doc.read_text()
for line in text.splitlines():assert len(re.findall(r'[\u4e00-\u9fff]',line))<=45,line
for target in re.findall(r'\]\(([^)]+)\)',text):assert (doc.parent/target).exists(),target
preserved=json.loads((root/'build/runs/20261031-053326-startup-context-d7dce6/preserved-user-files.json').read_text())
for name,digest in preserved.items():assert sha((root/name).read_bytes())==digest,name
subprocess.run(['git','diff','--check'],check=True,capture_output=True)
report=dict(status='passed-native-input-evidence',archiveFiles=len(manifest['files']),nativeDraws=5,debuggerTraces=4,specularReconstructions=2,exactAuthorTextureMatches=3,preservedUserFiles=4,visualAcceptance='failed-user-review')
(run/'input-phase-verification.json').write_bytes((json.dumps(report,indent=2)+'\n').encode());print(json.dumps(report))
