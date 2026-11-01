import argparse,hashlib,json,subprocess
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;archive='docs/acceptance/recovery/evidence/2026-11-01/view-fog-resource-reuse';dest=root/archive;assert json.loads((run/'verification.json').read_text())['status']=='passed'
parser=argparse.ArgumentParser();parser.add_argument('--resume',action='store_true');args=parser.parse_args();existing=set(subprocess.check_output(['git','diff','--cached','--name-only','--relative'],text=True).splitlines());assert args.resume or not existing
owned={'src/runtime/Level.cpp','src/scenes/CharacterSceneRenderer.cpp','tests/AssetLevelTests.cpp','tools/test_renderer_residency.py','tools/test_validation_protocol.py','README.md','CHANGELOG.md','docs/plans/azure-engine-plan.md','docs/plans/2026-11-01-view-fog-resource-reuse.md','docs/runtime/render-residency.md','docs/acceptance/recovery/view-fog-resource-reuse.md','docs/development/artifact-manifest.json'}
protected=json.loads((root/'build/runs/20261031-053326-startup-context-d7dce6/preserved-user-files.json').read_text());protected_names={name.replace('\\','/') for name in protected};sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for name,digest in protected.items():assert sha(root/name)==digest,name
expected=owned|{p.relative_to(root).as_posix() for p in dest.rglob('*') if p.is_file()};assert existing<=expected and not existing&protected_names
changed=set(subprocess.check_output(['git','diff','--name-only','--relative'],text=True).splitlines());untracked=set(subprocess.check_output(['git','ls-files','--others','--exclude-standard'],text=True).splitlines());assert changed-protected_names<=expected and untracked<=expected,(changed-protected_names-expected,untracked-expected)
paths=run/'git-add-paths.bin';paths.write_bytes(b''.join((':(literal)'+name).encode()+b'\0' for name in sorted(owned|{archive})));subprocess.run(['git','add','--pathspec-from-file='+str(paths),'--pathspec-file-nul'],check=True,capture_output=True)
assert set(subprocess.check_output(['git','diff','--cached','--name-only','--relative'],text=True).splitlines())==expected;subprocess.run(['git','diff','--cached','--check'],check=True)
m=json.loads((dest/'manifest.json').read_text());wanted={archive+'/'+row['path']:row['sha256'] for row in m['files']};wanted[archive+'/manifest.json']=sha(dest/'manifest.json');wanted[m['reusedEvidence']['path']]=m['reusedEvidence']['sha256']
for row in m['sourceChanges']:wanted[row['path']]=row['canonicalSha256']
frozen=json.loads((run/'release-build.json').read_text())['sourceFiles'];wanted.update(frozen)
for name in owned:wanted[name]=hashlib.sha256((root/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()
raw=subprocess.check_output(['git','cat-file','--batch'],input=''.join(':Project/AzureRender/'+name+'\n' for name in wanted).encode());cursor=0
for name,digest in wanted.items():
 end=raw.index(b'\n',cursor);header=raw[cursor:end].split();assert len(header)==3 and header[1]==b'blob',(name,header);n=int(header[2]);assert hashlib.sha256(raw[end+1:end+1+n]).hexdigest()==digest,name;cursor=end+1+n;assert raw[cursor:cursor+1]==b'\n';cursor+=1
assert cursor==len(raw)
report=dict(status='passed',stagedFiles=len(expected),evidenceIndexBlobs=len(m['files'])+1,compiledSourceIndexBlobs=len(frozen),preservedUserFiles=len(protected),unexpectedStagedFiles=[]);(run/'index-audit.json').write_bytes((json.dumps(report,indent=2)+'\n').encode());print(json.dumps(report))
