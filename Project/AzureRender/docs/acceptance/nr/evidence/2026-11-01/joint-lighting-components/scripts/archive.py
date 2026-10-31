import gzip,hashlib,json,re,subprocess
from pathlib import Path
from urllib.parse import unquote,urlsplit
root=Path.cwd();run=Path(__file__).resolve().parent;dest=root/'docs/acceptance/nr/evidence/2026-11-01/joint-lighting-components';dest.mkdir(parents=True,exist_ok=True)
sha=lambda raw:hashlib.sha256(raw).hexdigest()
summary=json.loads((run/'capture-summary.json').read_text());assert summary['status']=='captured-inputs-unchanged' and len(summary['cases'])==10
assert len(summary['inputs'])==37,len(summary['inputs'])
for name,digest in summary['inputs'].items():assert sha((root/name).read_bytes())==digest,name
comparison=json.loads((run/'component-comparison.json').read_text());assert comparison['status']=='measured' and len(comparison['pairs'])==5
for pair in comparison['pairs']:
    for region in pair['regions']:
        if region['region'].startswith('environment'):assert region['exactPixelsFraction']==1
        if pair['component']=='direct-diffuse' and not region['region'].startswith('environment'):assert region['jointMedianLinearDisplay']==0 and region['referenceMedianLinearDisplay']>0
protected=json.loads((root/'build/runs/20261031-053326-startup-context-d7dce6/preserved-user-files.json').read_text())
for name,digest in protected.items():assert sha((root/name).read_bytes())==digest,name
def copy(source,name=None):
    target=dest/(name or source.name);target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(source.read_bytes().replace(b'\r\n',b'\n'))
for name in ['capture-summary.json','component-comparison.json']:copy(run/name)
for name in ['capture_components.py','analyze_components.py','archive.py']:copy(run/name,'scripts/'+name)
for case in summary['cases']:
    folder=run/'captures'/case['name'];assert case['exitCode']==0 and case['status']=='captured'
    manifest=json.loads((folder/'frames/capture_manifest.json').read_text());assert manifest['capturedFrames']==1 and manifest['materialSurfaceProfileLayoutVersion']==17
    assert sha((folder/'frames/frame_000000.png').read_bytes())==case['imageSha256']
    log=(folder/'player.log').read_text(errors='replace');assert 'Allocator after unload: buffers=0 images=0' in log
    assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',log,re.I)
    for name in ['command.json','runtime.json']:copy(folder/name,case['name']+'/'+name)
    copy(folder/'frames/capture_manifest.json',case['name']+'/capture_manifest.json')
    target=dest/case['name']/'player.log.gz';target.write_bytes(gzip.compress((folder/'player.log').read_bytes(),mtime=0))
manifest_path=root/'docs/development/artifact-manifest.json';document=json.loads(manifest_path.read_text());entry={'path':'runs/'+run.name,'pathBase':'build','purpose':'同机位角色照明分项、原始帧及来源核验。','retention':'相应画质门禁完成并归档后另行清理。','pinMarker':'.keep','evidence':'docs/acceptance/nr/joint-lighting-components.md'}
existing=next((x for x in document['preservedRuns'] if x['path']==entry['path']),None)
if existing is None:document['preservedRuns'].append(entry)
else:assert existing==entry
manifest_path.write_bytes((json.dumps(document,ensure_ascii=False,indent=2)+'\n').encode('utf-8'));(run/'.keep').write_text('Joint lighting component audit; original native frames and source references retained.\n')
links=0
for name in ['README.md','CHANGELOG.md','docs/plans/azure-engine-plan.md','docs/acceptance/nr/joint-lighting-components.md','docs/plans/2026-11-01-material-radiance-normalization.md']:
    path=root/name;body=re.sub(r'```.*?```','',path.read_text(encoding='utf-8'),flags=re.S)
    for match in re.finditer(r'\[[^\]]*\]\(([^\s)]+)\)',body):
        target=urlsplit(match.group(1))
        if not target.scheme and target.path:assert (path.parent/unquote(target.path)).exists(),(name,target.path)
        links+=1
status={'status':'passed','visualAcceptance':'failed-user-review','scope':'Five same-pose component pairs with shared diagnostic exposure; complete actor/scene visual quality remains unaccepted.','captures':10,'frozenInputFiles':37,'protectedUserFiles':4,'environmentControlPairsExact':10,'linksChecked':links,'renderSourceCommit':'e759b52','renderPlayerSha256':summary['programSha256']}
(dest/'verification.json').write_bytes((json.dumps(status,indent=2)+'\n').encode('utf-8'))
files=[{'path':p.relative_to(dest).as_posix(),'bytes':p.stat().st_size,'sha256':sha(p.read_bytes())} for p in sorted(dest.rglob('*')) if p.is_file() and p.name!='manifest.json']
(dest/'manifest.json').write_bytes((json.dumps({'schemaVersion':1,'visualAcceptance':'failed-user-review','files':files},indent=2)+'\n').encode('utf-8'));print(json.dumps({'files':len(files),'bytes':sum(x['bytes'] for x in files),'captures':10,'links':links}))
