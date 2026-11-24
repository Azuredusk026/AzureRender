import argparse,gzip,hashlib,json,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[3];work=root/'build/evolution/p3'
folder=root/'docs/acceptance/p3/evidence/2026-10-07'
parser=argparse.ArgumentParser();parser.add_argument('--staged',action='store_true');args=parser.parse_args()
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
manifest=read(folder/'manifest.json')
assert manifest['status']=='passed' and manifest['tests']=={'Debug':150,'Release':152}
assert manifest['historicalByteAudit']['status']=='incomplete' and len(manifest['historicalByteAudit']['unverified'])==24
for name,h in manifest['files'].items():
 data=(folder/name).read_bytes();assert hashlib.sha256(data).hexdigest()==h,name
 if name in manifest['rawEvidenceEncodings']:
  encoding=manifest['rawEvidenceEncodings'][name];raw=gzip.decompress(data)
  assert len(raw)==encoding['originalBytes'] and hashlib.sha256(raw).hexdigest()==encoding['originalSha256'],name
inputs=0
for prefix in ('','before-replay-generator-fix/','before-lua-proxy-fix/','before-telemetry-storage-fix/'):
 receipt=read(folder/prefix/'source-snapshot/source-manifest.json')
 for name,record in receipt['files'].items():
  data=(folder/prefix/'source-snapshot'/name).read_bytes()
  assert len(data)==record['bytes'] and hashlib.sha256(data).hexdigest()==record['sha256'],name
 for name,h in receipt['buildInputs'].items():
  assert hashlib.sha256((folder/prefix/'source-snapshot'/name).read_bytes()).hexdigest()==h,name
  inputs+=1
for prefix in ('','before-replay-generator-fix/','before-lua-proxy-fix/','before-telemetry-storage-fix/'):
 for extra in ('','performance-first-final-failed/'):
  perf=folder/prefix/extra/'performance'
  if not (perf/'summary.json').exists():continue
  summary=read(perf/'summary.json')
  for load,files in summary['sceneHashes'].items():
   for name,h in files.items():assert hashlib.sha256((perf/load/name).read_bytes()).hexdigest()==h,(prefix,load,name)
if args.staged:
 repo=Path(subprocess.check_output(['git','rev-parse','--show-toplevel'],cwd=root).decode('utf-8').strip())
 mode=subprocess.check_output(['git','rev-parse','--show-object-format'],cwd=root).decode().strip()
 rows=subprocess.check_output(['git','ls-files','--stage','--full-name','-z'],cwd=root).split(b'\0');index={}
 for row in rows:
  if not row:continue
  info,name=row.split(b'\t',1);_,oid,stage=info.split();assert stage==b'0';index[name.decode('utf-8')]=oid.decode()
 for name in [*manifest['files'],'manifest.json','.gitattributes']:
  p=folder/name;data=p.read_bytes();key=p.relative_to(repo).as_posix()
  blob=b'blob '+str(len(data)).encode()+b'\0'+data
  assert index.get(key)==hashlib.new(mode,blob).hexdigest(),'Staged raw bytes differ: '+key
result={'status':'passed','reports':len(manifest['files']),'exactSnapshotInputs':inputs,'sourceTrees':4,'historicalByteLimits':24,'stagedRawBytesVerified':args.staged}
(work/('staged-evidence-integrity.json' if args.staged else 'evidence-integrity.json')).write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print(json.dumps(result),flush=True)
