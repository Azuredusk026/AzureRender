"""Verify normal orientation provenance, actual pixels and retained failures."""
import base64
import gzip
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import numpy as np
from PIL import Image

folder = Path(__file__).resolve().parent
root = next(p for p in folder.parents if (p/'CMakeLists.txt').is_file() and (p/'src').is_dir())
sha = lambda b: hashlib.sha256(b).hexdigest()
manifest = json.loads((folder/'manifest.json').read_text(encoding='utf8'))
files = {}
for bundle in manifest['bundles']:
    data = (folder/bundle['path']).read_bytes()
    assert sha(data) == bundle['sha256']
    rows = json.loads(gzip.decompress(data))['files']
    assert len(rows) == bundle['sourceFiles']
    for row in rows:
        data = row['data'].encode('utf8') if row['encoding'] == 'utf8' else base64.b64decode(row['data'])
        assert len(data) == row['bytes'] and sha(data) == row['sha256']
        files[row['path']] = data
for row in manifest['directFiles']:
    data = (folder/row['path']).read_bytes()
    assert len(data) == row['bytes'] and sha(data) == row['sha256']
load = lambda n: json.loads(files[n])
proof = load('normal-basis-causality.json')
assert proof['mappedVertexNormalMaximumError'] < 3e-5
assert proof['mappedVertexTangentMaximumError'] < 3e-5
assert proof['baseColorMaximumError'] < 1e-4
assert proof['currentMaximumNormalError'] > .07 and proof['correctedPredictionMaximumNormalError'] < .004
before = load('static-native-comparison.json')
after = load('normal-y-native-comparison.json')
assert before['thresholds'] == after['thresholds'] == dict(ssim=.95, medianLuminanceRelativeError=.1)
assert [r['event'] for r in after['cases']] == [889, 910, 928]
for a, b in zip(before['cases'], after['cases']):
    assert a['qualifiedPixels'] == b['qualifiedPixels'] and b['ssim'] > a['ssim']
assert [r['event'] for r in after['cases'] if not r['passed']] == [889, 928]
assert not load('sampler-only-native-comparison.json')['cases'][0]['passed']
assert 'FAILED (failures=4)' in files['normal-orientation-red.log'].decode('utf8')
assert 'FAILED (failures=1)' in files['normal-orientation-provenance-red.log'].decode('utf8')
for row in load('final-normal-tools-regression.json'):
    assert row['exitCode'] == 0 and 'Ran 57 tests' in files[row['log']].decode('utf8')
for row in load('final-public-conversions.json'):
    assert row['sourceY'] == 'positive' and row['targetY'] == 'negative'
    assert row['toolSha256'] == sha(files['source/tools/normal_map_orientation.py'])
a = np.asarray(Image.open(folder/'static-qualified-capture/frames/frame_000000.png'))
b = np.asarray(Image.open(folder/'normal-y-capture/frames/frame_000000.png'))
mask = np.asarray(Image.open(folder/'native-hair-visible-mask.png').convert('L')) > 0
assert int(mask.sum()) == 17017 and np.array_equal(a[mask], b[mask])
assert np.array_equal(a[268:299, 1252:1312], b[268:299, 1252:1312])
receipt = manifest['compiledSourceReceipt']
path = root/receipt['path']
assert sha(path.read_bytes()) == receipt['sha256']
subprocess.run([sys.executable, '-X', 'utf8', str(path.parent/'verify.py')], cwd=root, check=True)
assert manifest['visualAcceptance'] == 'failed-user-review'
print('Verified source basis, two normal maps, strict hair compatibility and 57 tools cases per configuration; two native cloth regions remain failed.')
