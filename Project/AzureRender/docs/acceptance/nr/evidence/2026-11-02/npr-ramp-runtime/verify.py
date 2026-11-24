import base64,gzip,hashlib,json,subprocess
from pathlib import Path
folder=Path(__file__).resolve().parent
if folder.parent.name=='runs':
    root=folder.parents[2];folder=root/'docs/acceptance/nr/evidence/2026-11-02/npr-ramp-runtime'
else:root=next(p for p in folder.parents if (p/'CMakeLists.txt').is_file() and (p/'src').is_dir())
manifest=json.loads((folder/'manifest.json').read_text());sha=lambda data:hashlib.sha256(data).hexdigest();count=0
for bundle in manifest['bundles']:
    data=(folder/bundle['path']).read_bytes();assert sha(data)==bundle['sha256'],bundle['path']
    rows=json.loads(gzip.decompress(data))['files'];assert len(rows)==bundle['sourceFiles']
    for row in rows:
        assert not Path(row['path']).is_absolute() and '..' not in Path(row['path']).parts
        raw=row['data'].encode('utf8') if row['encoding']=='utf8' else base64.b64decode(row['data'],validate=True)
        assert len(raw)==row['bytes'] and sha(raw)==row['sha256'],row['path'];count+=1
for row in manifest['directFiles']:
    raw=(folder/row['path']).read_bytes();assert len(raw)==row['bytes'] and sha(raw)==row['sha256'],row['path']
for row in manifest['reusedEvidence']:
    raw=(root/row['path']).read_bytes()
    if row.get('normalization')=='lf':raw=raw.replace(b'\r\n',b'\n')
    assert sha(raw)==row['sha256'],row['path']
process=subprocess.Popen(['git','cat-file','--batch'],cwd=root,stdin=subprocess.PIPE,stdout=subprocess.PIPE)
try:
    for row in manifest['compiledSources']+manifest['toolSources']:
        if row['storage']=='gzip':
            raw=(folder/row['path']).read_bytes();assert sha(raw)==row['archiveSha256'],row['path'];raw=gzip.decompress(raw)
        else:
            process.stdin.write((manifest['baseCommit']+':'+row['gitPath']+'\n').encode());process.stdin.flush()
            header=process.stdout.readline().decode().strip().split();assert len(header)==3 and header[1]=='blob',row['sourcePath']
            raw=process.stdout.read(int(header[2]));assert process.stdout.read(1)==b'\n'
        if row['normalization']=='lf':raw=raw.replace(b'\r\n',b'\n')
        assert sha(raw)==row['sha256'],row['sourcePath']
finally:process.stdin.close();assert process.wait(timeout=30)==0
print('PASS archive:',count,'bundled files,',len(manifest['directFiles']),'native images,',len(manifest['compiledSources']),'compiled sources')
