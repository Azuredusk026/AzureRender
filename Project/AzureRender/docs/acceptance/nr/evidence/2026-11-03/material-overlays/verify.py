"""Verify the overlay tool evidence while retaining integration failures."""
import base64
import gzip
import hashlib
import io
import json
from pathlib import Path

folder=Path(__file__).resolve().parent
manifest=json.loads((folder/'manifest.json').read_text(encoding='utf-8'))
sha=lambda b:hashlib.sha256(b).hexdigest()
archives={}
for bundle in manifest['bundles']:
    raw=(folder/bundle['path']).read_bytes()
    assert len(raw)==bundle['bytes'] and sha(raw)==bundle['sha256']
    document=json.loads(gzip.decompress(raw))
    rows=document['files']
    assert len(rows)==bundle['files']
    files={}
    for row in rows:
        data=base64.b64decode(row.get('data',document.get('blobs',{}).get(row['sha256'])),validate=True)
        assert len(data)==row['bytes'] and sha(data)==row['sha256']
        assert row['path'] not in files
        files[row['path']]=data
    archives[bundle['path']]=files
frames=0
for config in ['release','debug']:
    files=archives['gpu-'+config+'.json.gz']
    report=json.loads(files['report.json'])
    assert report['status']=='passed' and report['descriptorExact']
    assert all(c['maximumLinearError']<=.008 for c in report['cases'])
    images=[p for p in files if '/frames/frame_' in p and p.endswith('.png')]
    assert len(images)==report['frames']==4
    frames+=len(images)
q=archives['qualification.json.gz']
for config in ['release','debug']:
    log=q['overlay-native-tools-'+config+'-verbose.log'].decode()
    assert 'Ran 62 tests' in log and '100% tests passed, 0 tests failed out of 1' in log
assert 'Ran 5 tests' in q['overlay-tool-green.log'].decode()
assert 'FAILED' in q['overlay-tool-red.log'].decode()
component=json.loads(q['opaque-stage-native-comparison.json'])
full=json.loads(q['view-facing-native-comparison.json'])
assert component['thresholds']==full['thresholds']==dict(ssim=.95,medianLuminanceRelativeError=.1)
assert all(c['passed'] for c in component['cases'])
assert not full['cases'][2]['passed']
assert json.loads(q['source-material-verification.json'])['animationStateExact']
assert len(json.loads(q['joint-material-capture.json'])['images'])==192
assert json.loads(q['overlay-tool-static-equivalence.json'])['binaryExact']
images=archives['images.json.gz']
assert images['view-facing-capture/frames/frame_000000.png']==images['tool-static-capture/frames/frame_000000.png']
from PIL import Image
import numpy as np
for i in range(4):
    load=lambda n:np.asarray(Image.open(io.BytesIO(images[f'{n}/frames/frame_{i:06d}.png']))).astype(int)
    delta=abs(load('joint-material-capture')-load('joint-material-reopen'))
    assert np.count_nonzero(np.any(delta!=0,axis=2))==(14 if i==3 else 0)
    assert delta.max()==(1 if i==3 else 0)
    assert np.array_equal(load('joint-material-reopen'),load('joint-material-five-frame-control'))
video=manifest['video'];raw=(folder/video['path']).read_bytes()
assert len(raw)==video['bytes'] and sha(raw)==video['sha256']
assert frames==manifest['gpuFrames']==8
assert manifest['strictReopen']=='failed' and manifest['visualAcceptance']=='failed-user-review'
print('Verified 8 GPU frames, 62 tool cases per configuration, 192-frame provenance and strict integration failures.')
