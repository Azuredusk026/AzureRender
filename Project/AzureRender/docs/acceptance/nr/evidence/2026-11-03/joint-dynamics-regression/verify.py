"""Verify bounded joint regression, fresh-process reopen and related evidence."""
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
load = lambda n: json.loads(files[n])
regression = load('regression-green.json')
assert {(r['configuration'], r['group']) for r in regression} == {
    ('release', 'cpu'), ('release', 'gpu'), ('debug', 'cpu'), ('debug', 'gpu')}
for row in regression:
    assert row['exitCode'] == 0
    count = 12 if row['group'] == 'cpu' else 2
    assert f'100% tests passed, 0 tests failed out of {count}' in files[row['log']].decode('utf8')
metrics = load('regression-gpu-metrics.json')
assert {r['configuration'] for r in metrics} == {'release', 'debug'}
for row in metrics:
    assert row['independent']['instances'] == 3 and row['independent']['resources'] == 2
    assert row['independent']['cpuComputeMeanDifference'] == [0, 0, 0, 0]
    assert row['independent']['validationErrors'] == row['opacity']['validationErrors'] == 0
    assert len(row['opacity']['captures']) == 24
    expected = row['configuration'] == 'debug'
    assert all(c['validationEnabled'] == expected for c in row['opacity']['captures'])
proof = load('joint-reopen-verification.json')
assert proof['status'] == 'passed-persisted-project-reopen'
assert len(proof['frames']) == 4 and all(f['firstSha256'] == f['reopenSha256'] for f in proof['frames'])
assert proof['animationInstances'] == 11 and proof['heroClip'] == 9
assert load('joint-motion-reopen/frames/capture_manifest.json')['screenReflectionHistoryValid']
assert manifest['ctestCasesPerConfiguration'] == 14 and manifest['capturedJointFrames'] == 400
for row in manifest['relatedManifests']:
    path = root/row['path']
    assert sha(path.read_bytes()) == row['sha256']
    subprocess.run([sys.executable, '-X', 'utf8', str(path.parent/'verify.py')], cwd=root, check=True)
assert manifest['visualAcceptance'] == 'failed-user-review'
print('Verified 28 CTest results, 400 joint frames and four exact reopened frames; formal visual acceptance remains open.')
