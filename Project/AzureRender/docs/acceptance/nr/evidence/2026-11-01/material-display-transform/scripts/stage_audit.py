import argparse,hashlib,json,subprocess
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent
parser=argparse.ArgumentParser();parser.add_argument('--resume',action='store_true');args=parser.parse_args()
assert json.loads((run/'verification.json').read_text())['status']=='passed'
existing_staged=set(subprocess.check_output(['git','diff','--cached','--name-only','--relative'],text=True).splitlines())
assert args.resume or not existing_staged,'Pre-existing staged changes require separate ownership handling'
protected=json.loads((root/'build/runs/20261031-053326-startup-context-d7dce6/preserved-user-files.json').read_text());protected_names={p.replace('\\','/') for p in protected}
for name,digest in protected.items():assert hashlib.sha256((root/name).read_bytes()).hexdigest()==digest
archive='docs/acceptance/nr/evidence/2026-11-01/material-display-transform'
new_files={'docs/acceptance/nr/material-display-transform.md','docs/runtime/material-display-transform.md','src/assets/MaterialDisplayTransform.hpp','tools/test_material_display_gpu.py','tests/MaterialDisplayViewGpuTests.cpp'}
modified=subprocess.check_output(['git','diff','HEAD','--name-only','--relative'],text=True).splitlines();owned=[p for p in modified if p not in protected_names]
untracked=subprocess.check_output(['git','ls-files','--others','--exclude-standard'],text=True).splitlines()
assert all(p in new_files or p.startswith(archive+'/') for p in untracked),untracked
targets=owned+sorted(new_files)+[archive]
expected=set(owned)|new_files|{p.relative_to(root).as_posix() for p in (root/archive).rglob('*') if p.is_file()}
assert not existing_staged&protected_names and existing_staged<=expected,'Resume may only retain owned staged paths'
pathspec=run/'git-add-paths.bin';pathspec.write_bytes(b''.join((':(literal)'+name).encode('utf-8')+b'\0' for name in targets))
result=subprocess.run(['git','add','--pathspec-from-file='+str(pathspec),'--pathspec-file-nul'],capture_output=True);(run/'git-add.log').write_bytes(result.stdout+result.stderr);assert result.returncode==0
staged=set(subprocess.check_output(['git','diff','--cached','--name-only','--relative'],text=True).splitlines())
assert staged==expected,(staged-expected,expected-staged);assert not staged&protected_names
subprocess.run(['git','diff','--cached','--check'],check=True)
manifest=json.loads((root/archive/'manifest.json').read_text());wanted={archive+'/'+row['path']:row['sha256'] for row in manifest['files']}
wanted[archive+'/manifest.json']=hashlib.sha256((root/archive/'manifest.json').read_bytes()).hexdigest()
for row in manifest['sourceChanges']:wanted[row['path']]=row['canonicalSha256']
frozen=json.loads((run/'release-final-build.json').read_text());wanted.update(frozen['sourceFiles'])
queries=list(wanted);request=''.join(':Project/AzureRender/'+name+'\n' for name in queries).encode()
raw=subprocess.check_output(['git','cat-file','--batch'],input=request);cursor=0
for name in queries:
    end=raw.index(b'\n',cursor);header=raw[cursor:end].split();assert len(header)==3 and header[1]==b'blob',(name,header)
    size=int(header[2]);blob=raw[end+1:end+1+size];assert hashlib.sha256(blob).hexdigest()==wanted[name],name
    cursor=end+1+size;assert raw[cursor:cursor+1]==b'\n';cursor+=1
assert cursor==len(raw)
report={'status':'passed','stagedFiles':len(staged),'evidenceIndexBlobs':len(manifest['files'])+1,'frozenSourceIndexBlobs':len(frozen['sourceFiles']),
    'preservedUserFiles':len(protected),'unexpectedStagedFiles':[],'evidenceManifestMatchesGitIndex':True,'compiledSourceMatchesGitIndex':True}
(run/'index-audit.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
