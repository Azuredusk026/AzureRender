import gzip,hashlib,json
from pathlib import Path
root=Path.cwd().resolve();run=Path(__file__).resolve().parent;dest=(root/'docs/acceptance/nr/evidence/2026-11-01/material-display-transform').resolve()
assert dest.is_relative_to(root) and dest.relative_to(root).as_posix()=='docs/acceptance/nr/evidence/2026-11-01/material-display-transform'
assert (dest/'manifest.json').is_file();removed=[]
for path in list(dest.rglob('*')):
 if not path.is_file():continue
 relative=path.relative_to(dest).as_posix();remove=False
 if path.name.endswith('.azmeta.gz'):
  original=path.name[:-len('.azmeta.gz')]
  remove=Path(original).suffix in {'.json','.jsonl','.log','.txt','.rgba'} or 'frames' in path.relative_to(dest).parts or 'captures' in path.relative_to(dest).parts
 if relative.startswith('fixture-preflight/') and path.name!='summary.json' and not (path.parent==dest/'fixture-preflight' and path.name.endswith('.log.gz')):remove=True
 if relative.startswith('native-captures/') and path.suffix=='.png':
  remove=relative not in {'native-captures/near-world-linear/frames/frame_000000.png','native-captures/near-world-temporal/frames/frame_000031.png'}
 if relative=='native-comparison.png':remove=True
 if remove:
  assert path.resolve().is_relative_to(dest);removed.append(dict(path=relative,bytes=path.stat().st_size));path.unlink()
report=dict(status='pruned-derived-duplicates',removedFiles=len(removed),removedBytes=sum(r['bytes'] for r in removed),rules='Generated capture sidecars and redundant intermediate images are omitted; all original inputs, captures and logs remain in the pinned source run.',removed=removed)
raw=(json.dumps(report,indent=2)+'\n').encode();(run/'evidence-retention.json').write_bytes(raw);(dest/'evidence-retention.json').write_bytes(raw)
script=Path(__file__);(dest/'scripts'/script.name).write_bytes(script.read_bytes().replace(b'\r\n',b'\n'));(dest/'scripts-raw'/(script.name+'.gz')).write_bytes(gzip.compress(script.read_bytes(),mtime=0))
manifest_path=dest/'manifest.json';manifest=json.loads(manifest_path.read_text())
manifest['files']=[dict(path=p.relative_to(dest).as_posix(),bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in sorted(dest.rglob('*')) if p.is_file() and p!=manifest_path]
manifest_path.write_bytes((json.dumps(manifest,indent=2)+'\n').encode());print(json.dumps(dict(files=len(manifest['files']),bytes=sum(r['bytes'] for r in manifest['files']),removedFiles=len(removed))))
