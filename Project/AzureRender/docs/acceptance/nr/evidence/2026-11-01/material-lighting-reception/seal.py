import gzip,hashlib,json
from pathlib import Path
run=Path(__file__).resolve().parent;dest=Path.cwd()/'docs/acceptance/nr/evidence/2026-11-01/material-lighting-reception'
assert json.loads((run/'verification.json').read_text())['status']=='passed'
(dest/'verification.json').write_bytes((run/'verification.json').read_bytes())
(dest/'logs/verify-driver.log.gz').write_bytes(gzip.compress((run/'verify-driver.log').read_bytes(),mtime=0))
p=dest/'manifest.json';m=json.loads(p.read_text());m['files']=[dict(path=q.relative_to(dest).as_posix(),bytes=q.stat().st_size,sha256=hashlib.sha256(q.read_bytes()).hexdigest()) for q in sorted(dest.rglob('*')) if q.is_file() and q!=p];p.write_bytes((json.dumps(m,indent=2)+'\n').encode());print('Sealed',len(m['files']))
