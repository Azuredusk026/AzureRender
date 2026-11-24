import gzip,hashlib,json
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;dest=root/'docs/acceptance/nr/evidence/2026-11-01/joint-playable-display';assert json.loads((run/'verification.json').read_text())['status']=='passed'
(dest/'verification.json').write_bytes((run/'verification.json').read_bytes())
for name in ['verify-driver.log']:
 p=run/name
 if p.exists():(dest/'logs'/(name+'.gz')).write_bytes(gzip.compress(p.read_bytes(),mtime=0))
p=dest/'manifest.json';m=json.loads(p.read_text());m['files']=[dict(path=f.relative_to(dest).as_posix(),bytes=f.stat().st_size,sha256=hashlib.sha256(f.read_bytes()).hexdigest()) for f in sorted(dest.rglob('*')) if f.is_file() and f!=p];p.write_bytes((json.dumps(m,indent=2)+'\n').encode());print('Sealed',len(m['files']),'evidence files')
