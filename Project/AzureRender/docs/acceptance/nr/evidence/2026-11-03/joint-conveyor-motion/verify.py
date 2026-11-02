"""Verify retained conveyor content, execution and compiled source provenance."""
import base64
import gzip
import hashlib
import json
from pathlib import Path
import subprocess
import sys

folder = Path(__file__).resolve().parent
root = next(p for p in folder.parents if (p/'CMakeLists.txt').is_file() and (p/'src').is_dir())
read = lambda p: json.loads(p.read_text(encoding='utf8'))
sha = lambda b: hashlib.sha256(b).hexdigest()
manifest = read(folder/'manifest.json')
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
proof = load('conveyor-verification.json')
assert proof['generatedSlots'] == 91 and proof['capturedTemplates'] == 22
assert len(proof['emitters']) == 10 and proof['modelScale'] == [1, 1, 1]
assert proof['distanceScale'] == .85
assert all(e['originalBinaryPrefixExact'] and e['materialAndTextureDeclarationsExact'] for e in proof['emitters'])
assert not proof['nativeActivationVerified'] and not proof['nativeGlobalPhaseVerified'] and not proof['nativeRandomVerified']
motion = load('joint-motion-capture.json')
static = load('static-capture.json')
assert motion['frames'] == 192 and motion['animationInstances'] == 11
assert motion['programSha256'] == static['programSha256'] == manifest['programSha256']
assert len(motion['images']) + len(static['images']) == manifest['capturedFrames'] == 204
runtime = load('joint-motion/runtime.json')
assert not runtime['scriptErrors'] and not runtime['presentationErrors']
assert len(runtime['animations']) == 11
receipt = manifest['compiledSourceReceipt']
path = root/receipt['path']
assert sha(path.read_bytes()) == receipt['sha256']
subprocess.run([sys.executable, '-X', 'utf8', str(path.parent/'verify.py')], cwd=root, check=True)
assert manifest['visualAcceptance'] == 'failed-user-review'
print('Verified 204 actual frames, 10 emitters and preserved captured templates; visual acceptance remains open.')
