import gzip,hashlib,json,re,shutil
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;p=run/'observe_gpu.py';raw=p.read_bytes();(run/'observer-before-log-scope.py.gz').write_bytes(gzip.compress(raw,mtime=0));folder=run/'related-gpu/release/object-shadow';log=(folder/'shadow/player.log').read_text();timestamps=[int(s) for s in re.findall(r'^\[(\d+)\]',log,re.M)];assert timestamps
rows=[json.loads(s) for s in (folder/'shadow/diagnostics/azureplayer.log.jsonl').read_text().splitlines()];bad=lambda r:r['level']=='error' or r['subsystem']=='validation' and r['level']=='warning';actual=[r for r in rows if r['timestamp_ms']>=timestamps[0]]
assert len(actual)>0 and not any(bad(r) for r in actual) and any(r['message']=='Allocator after unload: buffers=0 images=0' for r in actual)
record=dict(status='red-confirmed',reason='The append-only shared log included older Player invocations.',observerSha256=hashlib.sha256(raw).hexdigest(),allRecords=len(rows),historicalErrorsAndValidationWarnings=sum(bad(r) for r in rows),currentInvocationRecords=len(actual),currentInvocationErrorsAndValidationWarnings=0,stdoutTimeRange=[timestamps[0],timestamps[-1]])
(run/'observer-log-scope-red.json').write_bytes((json.dumps(record,indent=2)+'\n').encode());print(json.dumps(record))
target=run/'failed-observer/related-gpu/release/object-shadow';assert folder.resolve().is_relative_to(run.resolve()) and target.resolve().is_relative_to(run.resolve());target.parent.mkdir(parents=True,exist_ok=True);assert not target.exists();shutil.move(str(folder),str(target))
for p in [run/'related-gpu/release/object-shadow-audit.json',run/'related-gpu/release/object-shadow.log',run/'related-release.json',run/'related-release-driver.log',run/'regression-pipeline.json']:
    q=run/'failed-observer'/p.relative_to(run);q.parent.mkdir(parents=True,exist_ok=True);assert not q.exists();shutil.move(str(p),str(q))
