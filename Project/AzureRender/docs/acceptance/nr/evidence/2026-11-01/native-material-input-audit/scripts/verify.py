import gzip,hashlib,json,re,subprocess
from pathlib import Path
from urllib.parse import unquote,urlsplit
root=Path.cwd();run=Path(__file__).resolve().parent;dest=root/'docs/acceptance/nr/evidence/2026-11-01/native-material-input-audit';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
protected=json.loads((root/'build/runs/20261031-053326-startup-context-d7dce6/preserved-user-files.json').read_text())
for name,digest in protected.items():assert sha(root/name)==digest,name
audit=json.loads((run/'audit.json').read_text());assert audit['knownNativeBones']==97
for case,count in zip(audit['cases'],[1792,11564]):
    assert case['vertices']==case['mappedVertices']==count and case['unmappedJointNames']==[] and case['uvMaximumError']==0 and case['tangentWExact']
    assert case['positionMaxMetres']<3e-5 and case['normalMaximumDegrees']<.15 and case['tangentMaximumDegrees']<.0002
count=0
for folder,name in [('material-capture-2','reference'),('material-capture-3','current-head'),('material-capture-3','current-head-visibility-one'),('material-capture-3','current-head-default-rim')]:
    out=run/folder/name;manifest=json.loads((out/'frames/capture_manifest.json').read_text());assert manifest['capturedFrames']==32
    assert manifest['width']==1280 and manifest['height']==720
    log=(out/'player.log').read_text(errors='replace');assert 'Allocator after unload: buffers=0 images=0' in log
    assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',log,re.I)
    assert len(list((out/'frames').glob('frame_*.png')))==32;count+=32
comparison=json.loads((run/'material-ab-comparison.json').read_text())
for case in comparison['comparisons']:
    if case['case']=='current-head' and case['region'] in ['face-interior','left-eye','right-eye','hair-crown']:assert case['medianLuminanceRelativeError']<.01
assert comparison['rimPolicyDifference']=={'changedPixels':107,'maxChannelError':1,'meanAbsoluteChannelError':5.099826388888889e-05}
coverage=json.loads((run/'gi-coverage.json').read_text());assert len(coverage['cases'])==7
for point in coverage['cases']:assert point['weights']==[1,0,0] and not point['outsideAllLayers']
query=json.loads((run/'gi-query.json').read_text());assert query['status']=='finite-gpu-query' and len(query['cases'])==7
docs=['README.md','CHANGELOG.md','docs/acceptance/nr/native-material-input-audit.md','docs/plans/azure-engine-plan.md'];links=0
for name in docs:
    p=root/name;text=re.sub(r'```.*?```','',p.read_text(encoding='utf-8'),flags=re.S)
    for match in re.finditer(r'\[[^\]]*\]\(([^\s)]+)\)',text):
        target=urlsplit(match.group(1))
        if not target.scheme and target.path:assert (p.parent/unquote(target.path)).exists(),(name,target.path)
        links+=1
manifest=json.loads((dest/'manifest.json').read_text())
for item in manifest['files']:assert sha(dest/item['path'])==item['sha256'],item['path']
for name in ['e804-PS-cb2.bin','e804-skin-matrices.bin','e950-ps-cb2-binding.bin','frame-bone-comparison.json']:
    raw=gzip.decompress((dest/'native'/f'{name}.gz').read_bytes());source=root/'build/runs/20261022-212213-native-binding-restoration-d39ea8/frame10401'/name
    assert hashlib.sha256(raw).hexdigest()==sha(source)
status={'status':'passed','scope':'Input reconstruction, four production static-material captures and actual clipmap queries; combined visual acceptance remains failed.','verifiedFrames':count,'nativeVertices':13356,'nativeBones':97,'giQueries':7,'protectedUserFiles':4,'linksChecked':links,'evidenceFiles':len(manifest['files'])}
(run/'verification.json').write_text(json.dumps(status,indent=2)+'\n');print(json.dumps(status))
