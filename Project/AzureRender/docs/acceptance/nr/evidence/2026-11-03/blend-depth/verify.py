"""Verify public blend-depth evidence and retain the failed visual control."""
import base64
import gzip
import hashlib
import json
from pathlib import Path

folder=Path(__file__).resolve().parent
manifest=json.loads((folder/'manifest.json').read_text(encoding='utf-8'))
sha=lambda b:hashlib.sha256(b).hexdigest()
archives={}
for bundle in manifest['bundles']:
    raw=(folder/bundle['path']).read_bytes()
    assert len(raw)==bundle['bytes'] and sha(raw)==bundle['sha256']
    document=json.loads(gzip.decompress(raw));files={}
    assert len(document['files'])==bundle['files']
    for row in document['files']:
        data=base64.b64decode(document['blobs'][row['sha256']],validate=True)
        assert len(data)==row['bytes'] and sha(data)==row['sha256']
        assert row['path'] not in files
        files[row['path']]=data
    archives[bundle['path']]=files
frames=0
for config in ['release','debug']:
    for kind,expected in [('gpu',24),('layers',16),('fog',10)]:
        files=archives[kind+'-'+config+'.json.gz']
        report=json.loads(files['summary.json' if kind=='layers' else 'report.json'])
        assert report['status']=='passed' and report['descriptorExact']
        if kind=='gpu':assert all(c['maximumLinearError']<=.008 for c in report['cases'])
        else:assert report['maximumLinearError']<=.008
        images=[n for n in files if '/frames/frame_' in n and n.endswith('.png')]
        assert len(images)==expected
        frames+=len(images)
q=archives['qualification.json.gz']
assert 'error C2039' in q['red-build.log'].decode(errors='replace')
for config in ['release','debug']:
    for label,count in [('final-cpu',3),('affected-cpu',2),('authoring-cpu',5)]:
        assert f'100% tests passed, 0 tests failed out of {count}' in q[config+'-'+label+'.log'].decode(errors='replace')
    materials=json.loads(q[config+'-validator.log'])['materials']
    assert {m['name'] for m in materials}>={'Near','Far','Rear'}
    assert all(m['class']=='fabric' for m in materials if m['name'] in ['Near','Far'])
pose=json.loads(q['pose-depth-native-comparison.json'])
assert pose['thresholds']==dict(ssim=.95,medianLuminanceRelativeError=.1)
assert not pose['cases'][2]['passed']
assert json.loads(q['compare-enum.json'])['GreaterEqual']==5
assert frames==manifest['gpuFrames']==100
assert len(manifest['shaders'])==manifest['shaderCount']==523 and manifest['shaderByteIdentical']
assert manifest['apiVersion']==9 and manifest['materialLayoutVersion']==21
assert not manifest['poseDepthVisualPassed'] and manifest['visualAcceptance']=='failed-user-review'
print('Verified 100 actual GPU frames and 10 CPU checks per configuration; full visual acceptance remains open.')
