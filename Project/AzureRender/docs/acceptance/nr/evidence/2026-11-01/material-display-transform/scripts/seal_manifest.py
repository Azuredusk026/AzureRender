import hashlib,json
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;dest=root/'docs/acceptance/nr/evidence/2026-11-01/material-display-transform'
verification=run/'verification.json';assert json.loads(verification.read_text())['status']=='passed'
(dest/'verification.json').write_bytes(verification.read_bytes().replace(b'\r\n',b'\n'))
path=dest/'manifest.json';manifest=json.loads(path.read_text())
manifest['files']=[dict(path=p.relative_to(dest).as_posix(),bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in sorted(dest.rglob('*')) if p.is_file() and p!=path]
path.write_bytes((json.dumps(manifest,indent=2)+'\n').encode());print('Sealed',len(manifest['files']),'evidence files')
