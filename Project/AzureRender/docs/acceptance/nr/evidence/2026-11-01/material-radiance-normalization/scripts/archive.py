import gzip,hashlib,json,subprocess
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;dest=root/'docs/acceptance/nr/evidence/2026-11-01/material-radiance-normalization'
assert json.loads((run/'native-capture-summary.json').read_text())['status']=='captured-inputs-unchanged'
assert json.loads((run/'related-gpu-summary.json').read_text())['status']=='passed'
dest.mkdir(parents=True,exist_ok=True)
sha=lambda raw:hashlib.sha256(raw).hexdigest()
text_suffixes={'.py','.json','.azureproject','.azurelevel','.azurematerial','.azmeta','.txt'}
def copy(source,name=None):
    target=dest/(name or source.name);target.parent.mkdir(parents=True,exist_ok=True);raw=source.read_bytes()
    target.write_bytes(raw.replace(b'\r\n',b'\n') if source.suffix in text_suffixes else raw)
def zipped(source,name):
    target=dest/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(gzip.compress(source.read_bytes(),mtime=0))
for source in sorted(run.glob('*.json')):copy(source)
for source in sorted(run.glob('*.log')):zipped(source,'logs/'+source.name+'.gz')
for name in ['build_both.py','build_both-initial.py','run_cpu.py','boundary_red.py','prepare_native.py','capture_native.py','compare_native.py','audit_grade.py','related_gpu.py','verify_phase.py','archive.py']:
    copy(run/name,'scripts/'+name)
copy(root/'tools/test_material_radiance_gpu.py','scripts/test_material_radiance_gpu.py')
for name in ['test_npr_illumination_gpu.py','test_eye_npr_gpu.py','test_projected_hair_gpu.py','test_unlit_texture_layers_gpu.py','test_vertex_color_gpu.py']:
    copy(root/'tools'/name,'scripts/'+name)
for name in ['gpu-red','gpu-release','gpu-final-debug','gpu-final-release','gpu-verified-debug']:
    folder=run/name
    if (folder/'summary.json').exists():copy(folder/'summary.json',name+'/summary.json')
    for case in sorted(folder.iterdir()):
        if not case.is_dir():continue
        prefix=name+'/'+case.name+'/'
        for filename in ['command.json','runtime.json']:
            if (case/filename).exists():copy(case/filename,prefix+filename)
        if (case/'player.log').exists():zipped(case/'player.log',prefix+'player.log.gz')
        if (case/'captures/azureplayer.log.jsonl').exists():zipped(case/'captures/azureplayer.log.jsonl',prefix+'diagnostics.jsonl.gz')
        if (case/'frames/capture_manifest.json').exists():copy(case/'frames/capture_manifest.json',prefix+'capture_manifest.json')
    # Keep actual failed and corrected unlit inputs for the early return regression.
    if name in ['gpu-release','gpu-final-release','gpu-verified-debug']:
        for case in ['unlit-control','unlit-exposure','unlit-quarter']:
            copy(folder/case/'frames/frame_000000.png',name+'/'+case+'/frame_000000.png')
inputs=json.loads((run/'native-inputs.json').read_text())
for case in inputs['cases']:
    name=case['name'];folder=run/'native-captures'/name;prefix='native/'+name+'/'
    copy(folder/'command.json',prefix+'command.json');copy(folder/'frames/capture_manifest.json',prefix+'capture_manifest.json')
    zipped(folder/'runtime.json',prefix+'runtime.json.gz');zipped(folder/'player.log',prefix+'player.log.gz')
    zipped(folder/'captures/azureplayer.log.jsonl',prefix+'diagnostics.jsonl.gz')
    frames=sorted((folder/'frames').glob('frame_*.png'));assert len(frames)==case['captureFrames']
    copy(frames[-1],prefix+frames[-1].name)
    copy(Path(case['project']),'inputs/'+Path(case['project']).name)
    zipped(Path(case['level']),'inputs/levels/'+Path(case['level']).name+'.gz')
for dirname in ['restored-character-materials','normalized-character-materials']:
    for path in sorted((Path(inputs['project'])/'assets'/dirname).iterdir()):
        if path.is_file():copy(path,'inputs/'+dirname+'/'+path.name)
copy(run/'native-comparison.png')
for base in ['related-gpu','related-gpu-final']:
    for folder in sorted((run/base).iterdir()):
        for path in sorted(folder.iterdir()):
            if path.is_file() and path.suffix=='.log':zipped(path,base+'/'+folder.name+'/'+path.name+'.gz')
            if path.is_dir() and (path/'summary.json').exists():copy(path/'summary.json',base+'/'+folder.name+'/'+path.name+'/summary.json')
for path in sorted(dest.rglob('*')):
    if path.is_file() and path.suffix in text_suffixes:path.write_bytes(path.read_bytes().replace(b'\r\n',b'\n'))
files=[{'path':p.relative_to(dest).as_posix(),'bytes':p.stat().st_size,'sha256':sha(p.read_bytes())} for p in sorted(dest.rglob('*')) if p.is_file() and p.name!='manifest.json']
(dest/'manifest.json').write_bytes((json.dumps({'schemaVersion':1,'visualAcceptance':'failed-user-review','files':files},indent=2)+'\n').encode())
manifest=root/'docs/development/artifact-manifest.json';document=json.loads(manifest.read_text());entry={'path':'runs/'+run.name,'pathBase':'build',
    'purpose':'材质线性输出、双配置独立曝光及原生联合画质核验。','retention':'相应画质门禁完成并归档后另行清理。',
    'pinMarker':'.keep','primaryProject':'project/full-world-normalized.azureproject','evidence':'docs/acceptance/nr/material-radiance-normalization.md'}
existing=next((v for v in document['preservedRuns'] if v['path']==entry['path']),None)
if existing is None:
    document['preservedRuns'].append(entry)
    data=(json.dumps(document,indent=2,ensure_ascii=False)+'\n').encode('utf-8')
    with manifest.open('r+b') as output:output.write(data);output.truncate()
else:assert existing==entry
print(json.dumps({'files':len(files),'bytes':sum(x['bytes'] for x in files)}))
