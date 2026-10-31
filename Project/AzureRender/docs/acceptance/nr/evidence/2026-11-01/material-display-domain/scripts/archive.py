import gzip,hashlib,json,subprocess
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;source=root/'build/runs/20261101-021345-material-radiance-normalization-1e6231'
dest=root/'docs/acceptance/nr/evidence/2026-11-01/material-display-domain';dest.mkdir(parents=True,exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def copy(path,name=None):
    target=dest/(name or path.name);target.parent.mkdir(parents=True,exist_ok=True);raw=path.read_bytes()
    target.write_bytes(raw.replace(b'\r\n',b'\n') if path.suffix in ['.json','.py','.azureproject','.azurematerial','.azmeta'] else raw)
def zipped(path,name):
    target=dest/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(gzip.compress(path.read_bytes(),mtime=0))
capture=json.loads((run/'grade-only-capture.json').read_text());assert capture['status']=='captured-inputs-unchanged'
inputs=json.loads((run/'grade-only-inputs.json').read_text())
for name,digest in inputs['inputs'].items():assert sha(root/name)==digest,name
assert sha(root/'build/ninja-msvc-release/AzurePlayer.exe')==capture['programSha256']
assert capture['shaderSha256']=={p.name:sha(p) for p in sorted((root/'build/ninja-msvc-release/shaders').glob('*.spv'))}
for path in sorted(run.glob('*.json')):copy(path)
for path in sorted(run.glob('*.py')):copy(path,'scripts/'+path.name)
for path in sorted(run.glob('*.log')):zipped(path,'logs/'+path.name+'.gz')
folder=run/'grade-only-capture';copy(folder/'command.json','capture/command.json');copy(folder/'frames/capture_manifest.json','capture/capture_manifest.json')
copy(folder/'frames/frame_000000.png','capture/frame_000000.png');zipped(folder/'runtime.json','capture/runtime.json.gz')
zipped(folder/'player.log','capture/player.log.gz');zipped(folder/'captures/azureplayer.log.jsonl','capture/diagnostics.jsonl.gz')
copy(source/'native-captures/near-normalized-isolated/frames/frame_000000.png','control/frame_000000.png')
copy(source/'native-captures/near-normalized-isolated/frames/capture_manifest.json','control/capture_manifest.json')
for name in ['near-normalized-isolated.azurelevel','near-grade-only.azurelevel']:zipped(run/'project/scene'/name,'inputs/'+name+'.gz')
copy(Path(inputs['project']),'inputs/near-grade-only.azureproject')
provenance={'sourceCommit':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'frozenBuildSourceCommit':json.loads((source/'frozen-build.json').read_text())['sourceCommit'],
    'sourceBuildIndexAudit':json.loads((source/'index-audit.json').read_text()),'visualAcceptance':'failed-user-review','changedProductionFiles':[]}
(dest/'source.json').write_text(json.dumps(provenance,indent=2)+'\n',encoding='utf-8',newline='\n')
files=[{'path':p.relative_to(dest).as_posix(),'bytes':p.stat().st_size,'sha256':sha(p)} for p in sorted(dest.rglob('*')) if p.is_file() and p.name!='manifest.json']
(dest/'manifest.json').write_bytes((json.dumps({'schemaVersion':1,'visualAcceptance':'failed-user-review','files':files},indent=2)+'\n').encode())
manifest=root/'docs/development/artifact-manifest.json';document=json.loads(manifest.read_text());entry={'path':'runs/'+run.name,'pathBase':'build','purpose':'角色调色单变量、颜色可达边界及显示策略输入核验。',
    'retention':'相应画质门禁完成并归档后另行清理。','pinMarker':'.keep','primaryProject':'project/near-grade-only.azureproject','evidence':'docs/acceptance/nr/material-display-domain.md'}
existing=next((x for x in document['preservedRuns'] if x['path']==entry['path']),None)
if existing is None:
    document['preservedRuns'].append(entry);data=(json.dumps(document,indent=2,ensure_ascii=False)+'\n').encode()
    with manifest.open('r+b') as stream:stream.write(data);stream.truncate()
else:assert existing==entry
(run/'.keep').write_text('Display-domain source audit; docs/acceptance/nr/material-display-domain.md\n')
print(json.dumps({'files':len(files),'bytes':sum(x['bytes'] for x in files)}))
