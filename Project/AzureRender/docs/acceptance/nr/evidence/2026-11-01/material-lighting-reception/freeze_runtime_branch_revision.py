import gzip, hashlib, json
from pathlib import Path
run=Path(__file__).resolve().parent
root=run.parents[2]
target=run/'runtime-branch-revision'
target.mkdir(exist_ok=True)
rows={}
for name in ['release-build.json','debug-build.json']:
    report=json.loads((run/name).read_text())
    for file,digest in report['sourceFiles'].items():
        data=(root/file).read_bytes().replace(b'\r\n',b'\n')
        assert hashlib.sha256(data).hexdigest()==digest,file
        dst=target/'source'/f'{file}.gz'
        dst.parent.mkdir(parents=True,exist_ok=True)
        dst.write_bytes(gzip.compress(data,mtime=0))
        rows[file]=digest
    (target/name).write_bytes((run/name).read_bytes())
(target/'manifest.json').write_text(json.dumps(dict(status='frozen-before-compile-time-lighting-family',files=rows),indent=2)+'\n')
print('Frozen source files:',len(rows))
