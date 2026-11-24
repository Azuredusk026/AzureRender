"""Verify archived coverage evidence without mutable build/run dependencies."""
import base64,gzip,hashlib,json,subprocess
from pathlib import Path
folder=Path(__file__).resolve().parent
root=next(path for path in folder.parents if (path/'CMakeLists.txt').is_file() and (path/'src').is_dir())
manifest=json.loads((folder/'manifest.json').read_text(encoding='utf8'));sha=lambda value:hashlib.sha256(value).hexdigest();count=0;seen={}
for bundle in manifest['bundles']:
    data=(folder/bundle['path']).read_bytes();assert sha(data)==bundle['sha256'],bundle['path']
    entries=json.loads(gzip.decompress(data))['files'];assert len(entries)==bundle['sourceFiles']
    for entry in entries:
        raw=entry['data'].encode('utf8') if entry['encoding']=='utf8' else base64.b64decode(entry['data'])
        assert len(raw)==entry['bytes'] and sha(raw)==entry['sha256'],entry['path']
        assert entry['path'] not in seen or seen[entry['path']]==entry['sha256'],entry['path'];seen[entry['path']]=entry['sha256'];count+=1
for entry in manifest['directFiles']:
    data=(folder/entry['path']).read_bytes();assert sha(data)==entry['sha256'] and len(data)==entry['bytes'],entry['path']
git=subprocess.Popen(['git','cat-file','--batch'],cwd=root,stdin=subprocess.PIPE,stdout=subprocess.PIPE)
try:
    for entry in manifest['compiledSources']:
        if entry['storage']=='gzip':
            compressed=(folder/entry['path']).read_bytes();assert sha(compressed)==entry['archiveSha256'];data=gzip.decompress(compressed)
        else:
            git.stdin.write((manifest['baseCommit']+':'+entry['gitPath']+'\n').encode());git.stdin.flush();header=git.stdout.readline().split();assert len(header)==3 and header[1]==b'blob',entry['gitPath'];data=git.stdout.read(int(header[2]));assert git.stdout.read(1)==b'\n'
        assert sha(data)==entry['sha256'],entry['sourcePath']
finally:git.stdin.close();git.stdout.close();git.wait()
for entry in manifest['toolSources']:
    data=(folder/entry['path']).read_bytes();assert sha(data)==entry['archiveSha256'];assert sha(gzip.decompress(data))==entry['sha256'],entry['sourcePath']
assert manifest['visualAcceptance']=='failed-user-review' and manifest['generatedMipCoverage']=='pending'
print('Verified',count,'bundled files,',len(manifest['compiledSources']),'compiled sources,',len(manifest['directFiles']),'native images; generated mips and joint visual acceptance remain open.')
