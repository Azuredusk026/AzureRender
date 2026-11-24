import argparse,hashlib,json,subprocess
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;archive='docs/acceptance/nr/evidence/2026-11-01/joint-playable-display';dest=root/archive
assert json.loads((run/'verification.json').read_text())['status']=='passed'
assert json.loads((run/'review.json').read_text())['status']=='behavior-cross-configuration-passed'
assert json.loads((run/'native-effects-summary.json').read_text())['status']=='measured'
assert json.loads((run/'native-character-effects.json').read_text())['characterResponseBudgetPassed']
assert json.loads((run/'object-shadow-summary.json').read_text())['responseBudgetPassed']
assert json.loads((run/'object-shadow-summary.json').read_text())['hasNonzeroSkinResponse']
assert json.loads((run/'native-entry/summary.json').read_text())['pixelExact']
assert all(row['interiorCriteriaPassed'] for row in json.loads((run/'standalone-comparison.json').read_text())['cases'])
parser=argparse.ArgumentParser();parser.add_argument('--resume',action='store_true');args=parser.parse_args()
existing=set(subprocess.check_output(['git','diff','--cached','--name-only','--relative'],text=True).splitlines());assert args.resume or not existing,'Handle pre-existing index changes separately'
owned={'README.md','CHANGELOG.md','docs/development/artifact-manifest.json','docs/plans/azure-engine-plan.md','docs/plans/2026-11-01-joint-playable-display.md','docs/acceptance/nr/joint-playable-display.md'}
protected=json.loads((root/'build/runs/20261031-053326-startup-context-d7dce6/preserved-user-files.json').read_text());sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
protected_names={name.replace('\\','/') for name in protected}
for name,digest in protected.items():assert sha(root/name)==digest
expected=owned|{p.relative_to(root).as_posix() for p in dest.rglob('*') if p.is_file()}
assert existing<=expected and not existing&protected_names,'Resume may only retain owned paths'
changed=set(subprocess.check_output(['git','diff','--name-only','--relative'],text=True).splitlines())
untracked=set(subprocess.check_output(['git','ls-files','--others','--exclude-standard'],text=True).splitlines())
assert changed-protected_names<=expected and untracked<=expected,(changed-protected_names-expected,untracked-expected)
paths=run/'git-add-paths.bin';paths.write_bytes(b''.join((':(literal)'+name).encode()+b'\0' for name in sorted(owned|{archive})))
subprocess.run(['git','add','--pathspec-from-file='+str(paths),'--pathspec-file-nul'],check=True,capture_output=True)
assert set(subprocess.check_output(['git','diff','--cached','--name-only','--relative'],text=True).splitlines())==expected
subprocess.run(['git','diff','--cached','--check'],check=True)
manifest=json.loads((dest/'manifest.json').read_text());wanted={archive+'/'+row['path']:row['sha256'] for row in manifest['files']};wanted[archive+'/manifest.json']=sha(dest/'manifest.json')
wanted[manifest['reusedEvidence']['path']]=manifest['reusedEvidence']['sha256']
build=json.loads((run/'build-reuse.json').read_text());frozen=build['configurations']['release']['sourceFiles'];wanted.update(frozen)
for name in owned:wanted[name]=hashlib.sha256((root/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()
queries=list(wanted);raw=subprocess.check_output(['git','cat-file','--batch'],input=''.join(':Project/AzureRender/'+name+'\n' for name in queries).encode());cursor=0
for name in queries:
 end=raw.index(b'\n',cursor);header=raw[cursor:end].split();assert len(header)==3 and header[1]==b'blob',(name,header)
 size=int(header[2]);assert hashlib.sha256(raw[end+1:end+1+size]).hexdigest()==wanted[name],name;cursor=end+1+size;assert raw[cursor:cursor+1]==b'\n';cursor+=1
assert cursor==len(raw)
record=dict(status='passed',stagedFiles=len(expected),frozenSourceIndexBlobs=len(frozen),evidenceIndexBlobs=len(manifest['files'])+1,preservedUserFiles=len(protected),unexpectedStagedFiles=[])
(run/'index-audit.json').write_bytes((json.dumps(record,indent=2)+'\n').encode());print(json.dumps(record))
