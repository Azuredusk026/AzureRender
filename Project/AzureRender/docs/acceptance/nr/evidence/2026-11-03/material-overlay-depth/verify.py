"""Verify the self-contained surface depth evidence and declared scope."""
import base64
import gzip
import hashlib
import json
from pathlib import Path

folder=Path(__file__).resolve().parent
manifest=json.loads((folder/'manifest.json').read_text(encoding='utf-8'))
sha=lambda value:hashlib.sha256(value).hexdigest()
archives={}
for bundle in manifest['bundles']:
    raw=(folder/bundle['path']).read_bytes()
    assert len(raw)==bundle['bytes'] and sha(raw)==bundle['sha256'],bundle['path']
    rows=json.loads(gzip.decompress(raw))['files']
    assert len(rows)==bundle['files']
    files={}
    for row in rows:
        data=base64.b64decode(row['data'],validate=True)
        assert len(data)==row['bytes'] and sha(data)==row['sha256'],row['path']
        assert row['path'] not in files
        files[row['path']]=data
    archives[bundle['path']]=files

frames=0
for name in ['release-full','debug-full','debug-mixed','release-fog','debug-fog']:
    files=archives[name+'.json.gz']
    summary=json.loads(files['report.json' if name.endswith('fog') else 'summary.json'])
    assert summary['status']=='passed' and summary['descriptorExact']
    assert summary['maximumLinearError']<=.008
    images=[p for p in files if p.endswith('.png') and '/frames/frame_' in p]
    expected=len(summary['cases']) if name.endswith('fog') else summary['frames']
    assert len(images)==expected,(name,len(images),expected)
    frames+=expected
    fingerprints=summary['inputFingerprints']
    player=next(v for k,v in fingerprints.items() if k.endswith('AzurePlayer.exe'))
    config='debug' if name.startswith('debug') else 'release'
    assert player==manifest['verification']['programs'][config]['sha256']

qualification=archives['qualification.json.gz']
trace=qualification['native-cloth-trace-qualified-1449-868.json']
assert sha(trace)==manifest['sourceTraceSha256']
assert json.loads(trace)['complete']
for config in ['release','debug']:
    log=qualification['depth-final-'+config+'-cpu.log'].decode(errors='replace')
    assert '100% tests passed, 0 tests failed out of 3' in log
v=manifest['verification']
assert v['shaderByteIdentical'] and v['shaderCount']==len(v['shaders'])==523
assert v['apiVersion']==9 and v['materialLayoutVersion']==21
assert frames==manifest['gpuFrames']==198
assert manifest['visualAcceptance']=='failed-user-review'
print('Verified 198 actual GPU frames, 6 CPU checks, source trace and archived inputs; joint visual acceptance remains open.')
