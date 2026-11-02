import gzip,hashlib,json,subprocess
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;dest=root/'docs/acceptance/nr/evidence/2026-11-01/material-lighting-reception';assert not dest.exists();dest.mkdir(parents=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert json.loads((run/'regression-pipeline.json').read_text())['status']=='passed'
def copy(p,name,raw=False):
    q=dest/name;q.parent.mkdir(parents=True,exist_ok=True);data=p.read_bytes()
    q.write_bytes(gzip.compress(data,mtime=0) if raw else data.replace(b'\r\n',b'\n') if p.suffix in {'.py','.json','.md'} else data)
for p in sorted(run.iterdir()):
    if not p.is_file():continue
    if p.suffix in {'.py','.json','.png'}:
        copy(p,p.name)
        if p.suffix in {'.py','.json'}:copy(p,'raw/'+p.name+'.gz',True)
    elif p.suffix in {'.log','.gz'}:copy(p,'logs/'+p.name+('.gz' if p.suffix!='.gz' else ''),p.suffix!='.gz')
groups=['before','before-field','build-attempts','public-gpu','public-gpu-failed-fixture','public-gpu-failed-isolation','public-object-smoke','public-object-smoke-corrected','red-public-gpu','probe-coupling-red','related-gpu','native-captures','route-release','route-debug','failed-effect-enum-source-environment-control-source-shadow-off','source-environment-control','source-environment-control-no-ssr','source-environment-control-no-ssr-shadow-off','source-environment-control-source-shadow-off']
groups+=['failed-observer','compatibility-repeat','determinism-captures']
for group in groups:
    folder=run/group
    if not folder.exists():continue
    for p in sorted(folder.rglob('*')):
        if not p.is_file() or '__pycache__' in p.parts:continue
        name=group+'/'+p.relative_to(folder).as_posix()
        if p.suffix in {'.gz'}:copy(p,name)
        elif p.suffix in {'.json','.jsonl','.log','.py','.azureproject','.azurelevel','.azurematerial','.azmeta','.gltf','.azenv','.bin','.aztex'}:copy(p,name+'.gz',True)
        elif p.suffix=='.png':
            # Complete image hashes remain in the capture reports. Native and
            # route sequences retain representative frozen sample frames here.
            if p.name.startswith('frame_'):
                frame=int(p.stem.split('_')[1]);native='native-captures' in p.parts
                route=any(part in ['route-release','route-debug'] for part in p.parts)
                if native and frame not in [0,5,11,31,63]:continue
                if route and frame not in [0,110,195,304,410,451,539]:continue
                if 'before-field' in p.parts and frame not in [0,31,63]:continue
            copy(p,name)
for group in ['project','candidate-project']:
    folder=run/group
    for pattern in ['*.azureproject','scene/*.azurelevel','assets/linear-display-materials/*.azurematerial','assets/linear-display-materials/*.azmeta']:
        for p in sorted(folder.glob(pattern)):copy(p,'inputs/'+group+'/'+p.relative_to(folder).as_posix()+'.gz',True)
build=json.loads((run/'release-build.json').read_text())
changed=set(subprocess.check_output(['git','diff','--name-only','--relative'],text=True).splitlines())|set(subprocess.check_output(['git','ls-files','--others','--exclude-standard'],text=True).splitlines())
source_names=set(build['sourceFiles'])|set(build['toolFiles'])
for name in sorted(changed&source_names):copy(root/name,'source/current/'+name+'.gz',True)
reused=['docs/acceptance/nr/evidence/2026-11-01/full-body-native-comparison/manifest.json','docs/acceptance/nr/evidence/2026-11-01/material-display-transform/manifest.json','docs/acceptance/nr/evidence/2026-11-01/joint-playable-display/manifest.json','docs/acceptance/recovery/evidence/2026-11-01/view-fog-resource-reuse/manifest.json']
m=dict(schemaVersion=1,baseCommit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),visualAcceptance='failed-user-review',referenceQualification='Frozen engine standalone controls; native whole-frame qualification remains pending.',reusedEvidence=[dict(path=p,sha256=sha(root/p)) for p in reused],files=[dict(path=p.relative_to(dest).as_posix(),bytes=p.stat().st_size,sha256=sha(p)) for p in sorted(dest.rglob('*')) if p.is_file()])
(dest/'manifest.json').write_bytes((json.dumps(m,indent=2)+'\n').encode());print('Archived',len(m['files']),'files')
