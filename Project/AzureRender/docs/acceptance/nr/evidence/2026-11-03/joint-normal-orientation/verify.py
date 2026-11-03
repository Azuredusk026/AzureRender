"""Verify the unchanged joint inputs, animation clocks and actual normal-map video."""
import base64
import gzip
import hashlib
import json
from pathlib import Path
import subprocess
import sys

folder = Path(__file__).resolve().parent
root = next(p for p in folder.parents if (p/'CMakeLists.txt').is_file() and (p/'src').is_dir())
sha = lambda b: hashlib.sha256(b).hexdigest()
manifest = json.loads((folder/'manifest.json').read_text(encoding='utf8'))


def unpack(bundle):
    data = (folder/bundle['path']).read_bytes()
    assert sha(data) == bundle['sha256']
    rows = json.loads(gzip.decompress(data))['files']
    assert len(rows) == bundle['sourceFiles']
    files = {}
    for row in rows:
        data = row['data'].encode('utf8') if row['encoding'] == 'utf8' else base64.b64decode(row['data'])
        assert len(data) == row['bytes'] and sha(data) == row['sha256']
        files[row['path']] = data
    return files


files = {}
for bundle in manifest['bundles']:
    files.update(unpack(bundle))
for row in manifest['directFiles']:
    data = (folder/row['path']).read_bytes()
    assert len(data) == row['bytes'] and sha(data) == row['sha256']
load = lambda n: json.loads(files[n])
proof = load('normal-orientation-verification.json')
assert proof['inputChecks'] == dict(unchangedImmutable=635, unchangedText=1093, changedNormalMaps=2)
assert proof['nativeHairPixelsExact'] and proof['facePixelsExact'] and proof['jointAnimationStatesExact']
capture = load('joint-normal-capture.json')
assert capture['frames'] == len(capture['images']) == manifest['capturedFrames'] == 192
runtime = load('joint-normal-capture/runtime.json')
assert len(runtime['animations']) == 11 and runtime['animationFrames'] == 2112
assert not runtime['scriptErrors'] and not runtime['presentationErrors']
assert load('joint-normal-capture/frames/capture_manifest.json')['screenReflectionHistoryValid']
comparison = load('joint-normal-qualified-comparison.json')
assert not comparison['strictEnvironmentRegionsPassed'] and not manifest['strictEnvironmentRegionsPassed']
assert comparison['environmentDifferentPixels'] == 1
for row in comparison['samples']:
    regions = {r['name']: r for r in row['regions']}
    assert regions['left-wall']['differentPixels'] == regions['ceiling']['differentPixels'] == 0
    assert regions['ground-reflection']['differentPixels'] > 1000
for row in load('final-normal-tools-regression.json'):
    assert row['exitCode'] == 0 and 'Ran 57 tests' in files[row['log']].decode('utf8')
video = json.loads((folder/'media/joint-normal-orientation.json').read_text())
assert video['frames'] == 192 and video['durationSeconds'] == 8 and video['width'] == 2560 and video['height'] == 1440
assert sha((folder/'media/joint-normal-orientation.mp4').read_bytes()) == video['sha256']
related = manifest['relatedManifest']
path = root/related['path']
assert sha(path.read_bytes()) == related['sha256']
subprocess.run([sys.executable, '-X', 'utf8', str(path.parent/'verify.py')], cwd=root, check=True)
assert manifest['visualAcceptance'] == 'failed-user-review'
print('Verified 192 actual frames, unchanged joint clocks and two corrected normal maps; strict environment and full visual acceptance remain open.')
