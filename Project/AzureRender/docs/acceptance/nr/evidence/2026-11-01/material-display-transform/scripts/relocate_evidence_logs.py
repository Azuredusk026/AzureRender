import gzip,hashlib,json
from pathlib import Path
root=Path.cwd().resolve();run=Path(__file__).resolve().parent;dest=(root/'docs/acceptance/nr/evidence/2026-11-01/material-display-transform').resolve()
assert dest.is_relative_to(root) and dest.relative_to(root).as_posix()=='docs/acceptance/nr/evidence/2026-11-01/material-display-transform'
rows=[]
for path in list(dest.rglob('azureplayer.log.jsonl.gz')):
 if path.parent.name!='captures':continue
 target=path.parent.parent/'diagnostics.jsonl.gz';assert path.resolve().is_relative_to(dest) and target.resolve().is_relative_to(dest) and not target.exists()
 digest=hashlib.sha256(path.read_bytes()).hexdigest();path.rename(target);assert hashlib.sha256(target.read_bytes()).hexdigest()==digest
 rows.append(dict(source=path.relative_to(dest).as_posix(),target=target.relative_to(dest).as_posix(),sha256=digest))
report=dict(status='relocated-owned-evidence',reason='Generated capture directories are ignored by the repository. Flat diagnostic filenames preserve exact compressed bytes.',files=rows)
raw=(json.dumps(report,indent=2)+'\n').encode();(run/'evidence-log-relocation.json').write_bytes(raw);(dest/'evidence-log-relocation.json').write_bytes(raw)
for name in ['relocate_evidence_logs.py','stage_audit.py']:
 script=run/name;(dest/'scripts'/name).write_bytes(script.read_bytes().replace(b'\r\n',b'\n'));(dest/'scripts-raw'/(name+'.gz')).write_bytes(gzip.compress(script.read_bytes(),mtime=0))
log=run/'stage-audit-driver.log';(dest/'logs/stage-audit-first-failed.log.gz').write_bytes(gzip.compress(log.read_bytes(),mtime=0))
print('Relocated',len(rows),'diagnostic streams without changing their bytes')
