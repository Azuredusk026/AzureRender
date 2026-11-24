import gzip,hashlib,json,re,subprocess,sys
from pathlib import Path
run=Path(__file__).resolve().parent;root=run.parents[2];folder=root/'docs/acceptance/nr/evidence/2026-11-01/material-lighting-reception'
manifest=json.loads((folder/'manifest.json').read_text())
for name in ['curate_archive.py','verify_family_scope.py','close_archive.py']:
    p=folder/'tools'/name;p.parent.mkdir(exist_ok=True);p.write_bytes((run/name).read_bytes().replace(b'\r\n',b'\n'))
for p in folder.rglob('*'):
    if p.is_file() and p.suffix in ['.json','.py','.md']:
        data=p.read_bytes();normalized=data.replace(b'\r\n',b'\n')
        if data!=normalized:p.write_bytes(normalized)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
manifest['files']=[dict(path=p.relative_to(folder).as_posix(),bytes=p.stat().st_size,sha256=sha(p)) for p in sorted(folder.rglob('*')) if p.is_file() and p!=folder/'manifest.json']
(folder/'manifest.json').write_bytes((json.dumps(manifest,indent=2)+'\n').encode())
for row in manifest['files']:assert sha(folder/row['path'])==row['sha256'],row['path']
for row in manifest['reusedEvidence']:assert sha(root/row['path'])==row['sha256'],row['path']
docs=['README.md','CHANGELOG.md','docs/plans/azure-engine-plan.md','docs/plans/2026-11-01-material-environment-reception.md','docs/runtime/material-lighting-reception.md','docs/runtime/material-display-transform.md','docs/runtime/character-material-profile.md','docs/acceptance/nr/material-lighting-reception.md']
for name in docs:
    path=root/name
    for target in re.findall(r'\]\(([^)]+)\)',path.read_text()):
        if target.startswith(('http:','https:','#')):continue
        assert (path.parent/target.split('#')[0]).exists(),(name,target)
print('Verified archive hashes, source reuse and final document links;',len(manifest['files']),'files')
