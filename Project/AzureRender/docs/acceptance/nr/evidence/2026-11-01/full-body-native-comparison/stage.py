import argparse,hashlib,json,subprocess
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;parser=argparse.ArgumentParser();parser.add_argument('--audit',action='store_true');args=parser.parse_args();archive='docs/acceptance/nr/evidence/2026-11-01/full-body-native-comparison';dest=root/archive;assert json.loads((run/'verification.json').read_text())['status']=='passed';owned={'README.md','CHANGELOG.md','docs/plans/azure-engine-plan.md','docs/plans/2026-11-01-full-body-native-comparison.md','docs/acceptance/nr/full-body-native-comparison.md','docs/development/artifact-manifest.json'};expected=owned|{p.relative_to(root).as_posix() for p in dest.rglob('*') if p.is_file()};protected=json.loads((root/'build/runs/20261031-053326-startup-context-d7dce6/preserved-user-files.json').read_text());sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for name,digest in protected.items():assert sha(root/name)==digest,name
protected_names={n.replace('\\','/') for n in protected};changed=set(subprocess.check_output(['git','diff','--name-only','--relative'],text=True).splitlines());untracked=set(subprocess.check_output(['git','ls-files','--others','--exclude-standard'],text=True).splitlines());assert not subprocess.check_output(['git','diff','--cached','--name-only']);assert changed-protected_names<=expected and untracked<=expected
if not args.audit:
 p=run/'git-add-paths.bin';p.write_bytes(b''.join((':(literal)'+n).encode()+b'\0' for n in sorted(owned|{archive})));subprocess.run(['git','add','--pathspec-from-file='+str(p),'--pathspec-file-nul'],check=True,capture_output=True);assert set(subprocess.check_output(['git','diff','--cached','--name-only','--relative'],text=True).splitlines())==expected;subprocess.run(['git','diff','--cached','--check'],check=True)
else:assert changed==protected_names and not untracked
m=json.loads((dest/'manifest.json').read_text());wanted={archive+'/'+r['path']:r['sha256'] for r in m['files']};wanted[archive+'/manifest.json']=sha(dest/'manifest.json')
for r in m['reusedEvidence']:wanted[r['path']]=r['sha256']
wanted.update(json.loads((root/'build/runs/20261101-101614-view-fog-reuse-814a54/release-build.json').read_text())['sourceFiles'])
for n in owned:wanted[n]=hashlib.sha256((root/n).read_bytes().replace(b'\r\n',b'\n')).hexdigest()
raw=subprocess.check_output(['git','cat-file','--batch'],input=''.join(':Project/AzureRender/'+n+'\n' for n in wanted).encode());cursor=0
for name,digest in wanted.items():
 end=raw.index(b'\n',cursor);header=raw[cursor:end].split();assert len(header)==3 and header[1]==b'blob',name;size=int(header[2]);assert hashlib.sha256(raw[end+1:end+1+size]).hexdigest()==digest,name;cursor=end+1+size+1
assert cursor==len(raw);result=dict(status='passed',auditOnly=args.audit,checkedIndexBlobs=len(wanted),stagedFiles=0 if args.audit else len(expected),preservedUserFiles=4,commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip());(run/('phase-completion.json' if args.audit else 'index-audit.json')).write_bytes((json.dumps(result,indent=2)+'\n').encode());print(json.dumps(result))
