import gzip,hashlib,json,re,shutil
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent
dest=root/'docs/acceptance/nr/evidence/2026-10-31/joint-visual-input-isolation';dest.mkdir(parents=True,exist_ok=True)
sha=lambda data:hashlib.sha256(data).hexdigest()
def textcopy(source,name=None):
    data=source.read_bytes().replace(b'\r\n',b'\n');out=dest/(name or source.name)
    out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(data)
def compressed(source,name):
    out=dest/name;out.parent.mkdir(parents=True,exist_ok=True)
    out.write_bytes(gzip.compress(source.read_bytes(),mtime=0))
for name in ['verification.json','summary.json','native-summary.json','context-summary.json','input-settings.json',
    'native-contexts.json','native-global-rows.json','source-scene-diff.json','scene-existing-image-comparison.json',
    'context-image-comparison.json','context-delta.json','cases.json','native-cases.json','context-cases.json',
    'decoder-red.log','decoder-green.log','decoder-debug.log','decoder-release.log','repository-audit.json','source-inputs.json','precommit.json']:
    textcopy(run/name)
for path in run.glob('*.py'):textcopy(path,'scripts/'+path.name)
cases=[]
for summary in ['summary.json','native-summary.json','context-summary.json']:
    cases+=json.loads((run/summary).read_text())['cases']
for case in cases:
    name=case['name'];folder=run/name
    textcopy(folder/'frames/capture_manifest.json',name+'/capture_manifest.json')
    compressed(folder/'runtime.json',name+'/runtime.json.gz')
    compressed(folder/'player.log',name+'/player.log.gz')
    descriptor=Path(case['project']);textcopy(descriptor,'inputs/'+descriptor.name)
    project=json.loads(descriptor.read_text());level=descriptor.parent/'scene'/project['startupScene'].split(':/',1)[1]
    compressed(level,'inputs/'+level.name+'.gz')
for name in ['control','native-scene-near']:
    shutil.copyfile(run/name/'frames/frame_000000.png',dest/(name+'.png'))
native=json.loads((run/'native-contexts.json').read_text())
for name in ['scene','portrait']:
    path=Path(native[name]['source']);shutil.copyfile(path,dest/(name+'-globals.bin'))
    receipt=path.with_name(path.stem+('-raw-receipt.json' if name=='scene' else '-receipt.json'))
    expected=json.loads(receipt.read_text())['data']['sha256'];assert sha(path.read_bytes())==expected
    textcopy(receipt,name+'-source-receipt.json')
reg=root/'build/runs/20261031-215800-full-candidate-regression-5b7fb7'
textcopy(reg/'status.json','full-regression-status.json')
for name in ['build-debug.log','build-release.log','ctest-debug.log']:
    compressed(reg/name,'full-regression/'+name+'.gz')
code={}
for name in ['tools/endfield_native/npr_lighting.py','tools/test_native_npr_lighting.py']:
    code[name]={'workingSha256':sha((root/name).read_bytes()),'lfSha256':sha((root/name).read_bytes().replace(b'\r\n',b'\n'))}
(dest/'source-code.json').write_text(json.dumps({'programSourceCommit':'39811d70cf282de2e8ad0e6ccf4d95bdd4201374','files':code},indent=2)+'\n')
(run/'.keep').write_text('联合画质未通过；保留原始诊断输入、六组画面及来源。\n',encoding='utf-8')
manifest=root/'docs/development/artifact-manifest.json';document=json.loads(manifest.read_text())
entry=dict(path='runs/'+run.name,pathBase='build',purpose='联合画质失败诊断、场景照明来源及同输入角色上下文。',retention='相应画质门禁完成并归档后另行清理。',pinMarker='.keep',primaryProject='project/native-scene-near.azureproject',evidence='docs/acceptance/nr/joint-visual-input-isolation.md')
existing=next((x for x in document['preservedRuns'] if x['path']==entry['path']),None)
if existing is None:document['preservedRuns'].append(entry)
else:assert existing==entry
manifest.write_text(json.dumps(document,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
files=[]
for path in sorted(dest.rglob('*')):
    if path.is_file() and path.name!='manifest.json':
        if path.suffix in {'.json','.log','.py'}:path.write_bytes(path.read_bytes().replace(b'\r\n',b'\n'))
        data=path.read_bytes();files.append({'file':path.relative_to(dest).as_posix(),'bytes':len(data),'sha256':sha(data)})
(dest/'manifest.json').write_text(json.dumps({'schemaVersion':1,'status':'diagnostic-inputs-verified-visual-acceptance-failed','retainedRun':str(run.relative_to(root)),'files':files},indent=2)+'\n')
print('Archived',len(files),'files,',sum(f['bytes'] for f in files),'bytes')
