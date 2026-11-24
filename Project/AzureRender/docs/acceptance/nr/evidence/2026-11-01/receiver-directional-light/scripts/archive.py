import gzip,hashlib,json,shutil
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent
dest=root/'docs/acceptance/nr/evidence/2026-11-01/receiver-directional-light';dest.mkdir(parents=True,exist_ok=True)
sha=lambda data:hashlib.sha256(data).hexdigest()
def copy(source,name=None):
    p=dest/(name or source.name);p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(source.read_bytes())
def zipcopy(source,name):
    p=dest/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(gzip.compress(source.read_bytes(),mtime=0))
for name in ['verification.json','frozen-build.json','remaining-status.json','character-cases.json','character-lighting-provenance.json',
    'display-energy-fit.json','display-balance-fit.json','display-curve-fit.json','native-frame-audit.json','original-material-frame.json','source-inputs.json','precommit.json','repository-audit.json']:
    if (run/name).exists():copy(run/name)
for path in run.glob('*.py'):copy(path,'scripts/'+path.name)
for name in ['build-final-release.log','build-final-debug.log','cpu-red.log','gpu-red.log','cpu-final-release.log','cpu-final-debug.log']:
    copy(run/name)
    zipcopy(run/name,name+'.gz')
for cfg in ['release','debug']:
    copy(run/f'gpu-final-{cfg}/summary.json',f'gpu-final-{cfg}-summary.json')
    for kind in ['hair','eye','object']:
        copy(run/f'{kind}-{cfg}/summary.json',f'{kind}-{cfg}-summary.json')
        zipcopy(run/f'{kind}-{cfg}.log',f'{kind}-{cfg}.log.gz')
copy(run/'gpu-final-release-all/summary.json','gpu-final-release-all-summary.json')
copy(run/'gpu-red/summary.json','gpu-red-summary.json')
for label,folder in [('red',run/'gpu-red/receiver-main'),('green',run/'gpu-final-release/receiver-main')]:
    copy(folder/'frames/frame_000000.png',label+'-receiver-main.png');copy(folder/'command.json',label+'-command.json')
    zipcopy(folder/'player.log',label+'-player.log.gz')
for summary in run.glob('character-summary-*.json'):copy(summary)
for folder in (run/'character-capture').iterdir():
    if not folder.is_dir():continue
    name=folder.name;copy(folder/'summary.json',name+'/summary.json')
    copy(folder/'frames/capture_manifest.json',name+'/capture_manifest.json')
    zipcopy(folder/'runtime.json',name+'/runtime.json.gz');zipcopy(folder/'player.log',name+'/player.log.gz')
    paths=sorted((folder/'frames').glob('frame_*.png'))
    (dest/name/'frames-manifest.json').write_text(json.dumps([{'file':p.name,'sha256':sha(p.read_bytes())} for p in paths],indent=2)+'\n')
for name in ['near-reference-light','near-reference-source-display']:
    copy(run/'character-capture'/name/'frames/frame_000000.png',name+'.png')
for case in json.loads((run/'character-cases.json').read_text()):
    project=Path(case['project']);copy(project,'inputs/'+project.name)
    d=json.loads(project.read_text());level=project.parent/'scene'/d['startupScene'].split(':/',1)[1]
    zipcopy(level,'inputs/'+level.name+'.gz')
native=root/'build/runs/20261022-212213-native-binding-restoration-d39ea8/frame10401/frame-bone-comparison.json'
copy(native,'native-frame-bone-comparison.json')
zipcopy(native,'native-frame-bone-comparison.json.gz')
manifest=root/'docs/development/artifact-manifest.json';document=json.loads(manifest.read_text())
entry=dict(path='runs/'+run.name,pathBase='build',purpose='主光接收范围修复、严格兼容及失败显示实验。',
    retention='相应画质门禁完成并归档后另行清理。',pinMarker='.keep',primaryProject='project/full-control.azureproject',evidence='docs/acceptance/nr/receiver-directional-light.md')
existing=next((x for x in document['preservedRuns'] if x['path']==entry['path']),None)
if existing is None:document['preservedRuns'].append(entry)
else:assert existing==entry
manifest.write_text(json.dumps(document,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
(run/'.keep').write_text('主光来源修复与显示诊断；联合画质未通过。\n',encoding='utf-8')
files=[]
for path in sorted(dest.rglob('*')):
    if not path.is_file() or path.name=='manifest.json':continue
    data=path.read_bytes()
    if path.suffix in {'.json','.log','.py','.azureproject'}:data=data.replace(b'\r\n',b'\n');path.write_bytes(data)
    if path.suffix=='.log':data=b'\n'.join(line.rstrip(b' \t') for line in data.split(b'\n'));path.write_bytes(data)
    files.append({'file':path.relative_to(dest).as_posix(),'bytes':len(data),'sha256':sha(data)})
(dest/'manifest.json').write_text(json.dumps({'schemaVersion':1,'status':'receiver-mechanism-verified-visual-acceptance-failed',
    'retainedRun':str(run.relative_to(root)),'files':files},indent=2)+'\n')
print('Archived',len(files),'files,',sum(x['bytes'] for x in files),'bytes')
