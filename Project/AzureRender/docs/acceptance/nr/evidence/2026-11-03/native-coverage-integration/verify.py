"""Verify scoped native integration, preserved failures and build provenance."""
import base64,gzip,hashlib,json,subprocess,sys
from pathlib import Path
folder=Path(__file__).resolve().parent
root=next(p for p in folder.parents if (p/'CMakeLists.txt').is_file() and (p/'src').is_dir())
manifest=json.loads((folder/'manifest.json').read_text(encoding='utf8'));sha=lambda b:hashlib.sha256(b).hexdigest();entries={}
for bundle in manifest['bundles']:
    data=(folder/bundle['path']).read_bytes();assert sha(data)==bundle['sha256']
    rows=json.loads(gzip.decompress(data))['files'];assert len(rows)==bundle['sourceFiles']
    for row in rows:
        data=row['data'].encode('utf8') if row['encoding']=='utf8' else base64.b64decode(row['data'])
        assert len(data)==row['bytes'] and sha(data)==row['sha256'],row['path'];entries[row['path']]=data
for row in manifest['directFiles']:
    data=(folder/row['path']).read_bytes();assert sha(data)==row['sha256'] and len(data)==row['bytes']
receipt=manifest['compiledSourceReceipt'];path=root/receipt['path'];assert sha(path.read_bytes())==receipt['sha256']
subprocess.run([sys.executable,'-X','utf8',str(path.parent/'verify.py')],cwd=root,check=True)
captures=json.loads(entries['native-final-captures.json']);build=json.loads(entries['lifecycle-release-build.json'])
assert captures['programSha256']==manifest['programSha256']==build['programs']['AzurePlayer']
assert sum(len(c['images']) for c in captures['captures'])==400 and len(captures['captures'])==11
metrics=json.loads(entries['native-final-verification.json']);assert metrics==manifest['metrics']
assert metrics['matchedBodyRegions']==36 and metrics['matchedPortraitRegions']==20
assert not metrics['nativeFullRegionPassed'] and metrics['originalBodyFailures']=={'0':[1,2,5],'3':[1,2,5]}
assert manifest['visualAcceptance']=='failed-user-review'
print('Verified',len(entries),'archived files, 400 captured frame identities, 3 images and scoped build provenance; full native hair and joint visual acceptance remain failed.')
