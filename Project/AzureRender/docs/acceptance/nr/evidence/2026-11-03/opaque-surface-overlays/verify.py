"""Verify overlay stages, actual captures and the preserved visual failure."""
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
    files=archives['overlay-'+config+'.json.gz']
    for aa in [0,4]:
        prefix='frozen-'+config+'-aa'+str(aa)+'/'
        report=json.loads(files[prefix+'report.json'])
        assert report['status']=='passed' and report['descriptorExact']
        assert report['frames']==16 and report['antiAliasing']==aa
        images=[n for n in files if n.startswith(prefix) and '/frames/frame_' in n and n.endswith('.png')]
        assert len(images)==16
        frames+=16
    files=archives['default-'+config+'.json.gz']
    for kind,count in [('layers',16),('fog',10),('blend',24),('tool',4)]:
        report=json.loads(files[kind+'/summary.json' if kind=='layers' else kind+'/report.json'])
        assert report['status']=='passed' and report['descriptorExact']
        images=[n for n in files if n.startswith(kind+'/') and '/frames/frame_' in n and n.endswith('.png')]
        assert len(images)==count
        frames+=count
    files=archives['opaque-fog-'+config+'.json.gz']
    assert len([n for n in files if '/frames/frame_' in n and n.endswith('.png')])==2
    frames+=2
q=archives['qualification.json.gz']
for config in ['release','debug']:
    for label,count in [('final-cpu',4),('authoring-cpu',6),('frozen-cpu',1)]:
        assert f'100% tests passed, 0 tests failed out of {count}' in q[config+'-'+label+'.log'].decode(errors='replace')
assert 'surface.submission' in q['red-cpu.log'].decode(errors='replace')
assert 'shadow depth ownership' in q['owner-red-cpu.log'].decode(errors='replace')
assert json.loads(q['opaque-fog-verification.json'])['status']=='passed'
assert all(c['maximumLinearError']==0 for c in json.loads(q['opaque-fog-verification.json'])['cases'])
assert json.loads(q['pose-owner-comparison.json'])['pixelExact']
pose=json.loads(q['pose-native-comparison.json'])
assert pose['thresholds']==dict(ssim=.95,medianLuminanceRelativeError=.1)
assert not pose['cases'][2]['passed']
joint=archives['joint.json.gz']
capture=json.loads(q['joint-overlay-capture.json'])
assert capture['frames']==192 and len(capture['images'])==192
for i in range(4):
    suffix=f'/frames/frame_{i:06d}.png'
    assert joint['joint-overlay-capture'+suffix]==joint['joint-overlay-reopen'+suffix]
verification=json.loads(q['joint-verification.json'])
assert verification['reopenStrictExact'] and verification['animationStateExact']
assert verification['fixedSteps']==477 and verification['animationInstances']==11
assert all(c['changedPixels']==0 for c in verification['regions'] if c['region']!='ground')
assert any(c['changedPixels']>0 for c in verification['regions'] if c['region']=='ground')
video=manifest['video'];raw=(folder/video['path']).read_bytes()
assert len(raw)==video['bytes'] and sha(raw)==video['sha256']
assert frames==manifest['gpuFrames']==176
assert manifest['shaderByteIdentical'] and len(manifest['shaders'])==manifest['shaderCount']==523
assert manifest['apiVersion']==9 and manifest['materialLayoutVersion']==21
assert not manifest['poseVisualPassed'] and manifest['visualAcceptance']=='failed-user-review'
print('Verified 176 mechanism frames, 192-frame provenance, four exact reopen frames and retained visual failure.')
