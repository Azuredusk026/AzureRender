import gzip,hashlib,json
from pathlib import Path
run=Path(__file__).resolve().parent;root=run.parents[2];folder=root/'docs/acceptance/nr/evidence/2026-11-01/material-lighting-reception'
manifest=json.loads((folder/'manifest.json').read_text());omitted=[]
for row in manifest['files']:
    name=row['path'];p=folder/name
    duplicateProject=name.startswith('repeat-isolations/projects/') and not name.endswith(('.azureproject.gz','.azurelevel.gz'))
    oldSequence=name.startswith(('compatibility-repeat/','determinism-captures/')) and p.name.startswith('frame_') and p.suffix=='.png' and p.stem not in ['frame_000000','frame_000003','frame_000009','frame_000010','frame_000011']
    if duplicateProject or oldSequence:
        assert p.resolve().is_relative_to(folder.resolve()) and p.is_file()
        omitted.append(row);p.unlink()
index=folder/'omitted-duplicate-copies.json.gz'
index.write_bytes(gzip.compress((json.dumps(dict(qualification='Original files remain in the pinned run. Repeated project asset copies and unsampled historical sequence frames use the recorded input and image hashes.',files=omitted),indent=2)+'\n').encode(),mtime=0))
prior=run/'runtime-branch-revision';priorManifest=json.loads((prior/'manifest.json').read_text())
complete=json.loads((run/'shader-family-revision/frozen-complete/release-build.json').read_text())['sourceFiles']
for name in ['release-build.json','debug-build.json','manifest.json']:
    p=folder/'prior-runtime-branch'/name;p.parent.mkdir(exist_ok=True);p.write_bytes((prior/name).read_bytes())
for name,digest in priorManifest['files'].items():
    if complete.get(name)==digest:continue
    p=folder/'prior-runtime-branch/source'/(name+'.gz');p.parent.mkdir(parents=True,exist_ok=True)
    p.write_bytes((prior/'source'/(name+'.gz')).read_bytes())
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
manifest['files']=[dict(path=p.relative_to(folder).as_posix(),bytes=p.stat().st_size,sha256=sha(p)) for p in sorted(folder.rglob('*')) if p.is_file() and p.name!='manifest.json']
(folder/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps(dict(files=len(manifest['files']),megabytes=round(sum(r['bytes'] for r in manifest['files'])/1e6,1),omittedCopies=len(omitted))))
