"""Verify idle timing, preserved inputs, actual video and failed import control."""
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
proof = load('idle-loop-verification-v2.json')
assert proof['checkedChannels'] == 939 and proof['sourceClips'] == 9 and proof['totalClips'] == 10
assert proof['originalGeometryAndTexturesExact'] and proof['firstSamplesExact'] and proof['allLoopEndpointsExact']
clip = load('idle-loop-output/hero-idle-loop-v2.glb.loop.json')
assert clip['startTime'] == 0.01666666753590107 and clip['endTime'] == 2.5166666507720947
assert abs(clip['duration'] - 2.5) < 1e-6 and clip['clipIndex'] == 9
assert sha(files['source/tools/bake_animation_loop.py']) == clip['toolSha256']
assert load('idle-loop-first-frame.json')['maximumDifference'] == 0
pose = load('idle-pose-verification.json')
assert pose['samples'] == 151 and all(n['closedEndpointDistanceMeters'] == 0 for n in pose['nodes'])
assert load('idle-face-tracks.json')['varyingTracks'] == 0
failed = load('idle-loop-import-red.json')
assert failed['status'] == 'rejected-invalid-generated-times' and failed['exitCode'] == 1
capture = load('joint-motion-loop-v2-capture.json')
assert capture['frames'] == len(capture['images']) == manifest['capturedFrames'] == 192
assert capture['programSha256'] == manifest['programSha256']
runtime = load('joint-motion-loop-v2/runtime.json')
assert len(runtime['animations']) == 11 and runtime['animationFrames'] == 2112
assert next(a for a in runtime['animations'] if a['node'] == 'hero')['clip'] == 9
assert all(a['clip'] == 0 for a in runtime['animations'] if a['node'] != 'hero')
assert not runtime['scriptErrors'] and not runtime['presentationErrors']
assert load('joint-motion-loop-v2/frames/capture_manifest.json')['screenReflectionHistoryValid']
receipt = manifest['compiledSourceReceipt']
path = root/receipt['path']
assert sha(path.read_bytes()) == receipt['sha256']
subprocess.run([sys.executable, '-X', 'utf8', str(path.parent/'verify.py')], cwd=root, check=True)
assert manifest['visualAcceptance'] == 'failed-user-review'
print('Verified 939 closed tracks, exact first frame and 192 actual frames; visual acceptance remains open.')
