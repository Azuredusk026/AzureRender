import json,hashlib,re,subprocess,shutil,gzip
from pathlib import Path
root=Path(__file__).resolve().parents[3];work=root/'build/evolution/p3';dest=root/'docs/acceptance/p3/evidence/2026-10-07'
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
assert read(work/'documents-closed.json')['status']=='passed'
assert read(work/'long-run/summary.json')['status']=='passed'
assert read(work/'final-package-result.json')['status']=='passed'
assert read(work/'source-snapshot.json')['status']=='passed'
dest.mkdir(parents=True,exist_ok=True);(dest/'.gitattributes').write_text('* -text -whitespace\n',encoding='utf-8')
reports={}
for p in work.iterdir():
 if p.is_file() and p.suffix in {'.log','.json','.py','.cpp','.hpp','.rsp'} and not p.name.startswith(('own-','recover','replay_')):
  reports[p.name]=p
for folder in ['performance','intel','inspector-package','generated-package','intel-procedural','installed-procedural',
 'managed-intel-coreclr','managed-intel-nativeaot','forced-validation-intel','forced-validation-installed','coverage','authoring','authoring-reopened','replay-probe','replay-probe-30s-failed','long-run','strict-history','source-snapshot','performance-first-final-failed','memory-observation-2','stale-receipts']:
 for p in (work/folder).rglob('*'):
  if not p.is_file():continue
  if folder=='source-snapshot':reports[p.relative_to(work).as_posix()]=p;continue
  if p.suffix in {'.log','.json','.csv'} and not any(x in p.parts for x in ('game','share','assets','bin','package')):
   reports[p.relative_to(work).as_posix()]=p
# Retain the failed launch and the complete superseded gate reports.
for archive_name in ('before-replay-generator-fix','before-lua-proxy-fix','before-telemetry-storage-fix'):
 previous=work/archive_name
 for p in previous.rglob('*'):
  if not p.is_file():continue
  relative=p.relative_to(previous).as_posix()
  if relative.startswith('source-snapshot/'):
   reports[archive_name+'/'+relative]=p;continue
  if p.suffix not in {'.log','.json','.csv'}:continue
  if any(part in p.parts for part in ('share','bin','game','assets','install-moved','packages')):continue
  if p.name.startswith(('own-','recover')):continue
  reports[archive_name+'/'+relative]=p
# Preserve the actual measured fixture bytes as well as their recorded hashes.
for base in (work/'performance',work/'performance-first-final-failed/performance',
             work/'before-replay-generator-fix/performance',work/'before-lua-proxy-fix/performance',
             work/'before-telemetry-storage-fix/performance',work/'before-telemetry-storage-fix/performance-first-final-failed/performance'):
 for p in base.rglob('*'):
  if p.is_file() and not any(part in {'.azure','__pycache__'} for part in p.parts):
   reports[p.relative_to(work).as_posix()]=p
release=root/'build/ninja-msvc-release';debug=root/'build/ninja-msvc-debug'
reports.update({'release-tests.log':release/'release-gate/test.log','release-result.json':release/'release-gate/result.json',
 'debug-provenance.json':debug/'build-provenance.json','release-provenance.json':release/'build-provenance.json'})
for config in ['debug','release']:
 base=root/('build/ninja-msvc-'+config)/'managed-workflows'
 for p in base.rglob('*'):
  if p.is_file() and p.suffix in {'.log','.json','.csv'} and not any(x in p.parts for x in ('game','share','assets','bin')):
   reports['workflows/'+config+'/'+p.relative_to(base).as_posix()]=p
for name in ['visual-regression-summary.json','visual-regression-legacy-summary.json','visual-regression-legacy-skinning-summary.json',
 'playable-package/evidence/summary.json','playable-package/evidence/cycles.summary.json']:
 reports['regression/'+name.replace('/','-')]=release/name
hashes={};encodings={}
for name,p in reports.items():
 data=p.read_bytes()
 if len(data)>32*1024*1024 and 'source-snapshot/' not in name:
  original_name=name;original_hash=hashlib.sha256(data).hexdigest();original_size=len(data)
  data=gzip.compress(data,compresslevel=6,mtime=0);assert hashlib.sha256(gzip.decompress(data)).hexdigest()==original_hash
  name+='.gz';encodings[name]={'originalPath':original_name,'encoding':'gzip','originalSha256':original_hash,'originalBytes':original_size}
 out=dest/name;out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(data);hashes[name]=hashlib.sha256(data).hexdigest()
proof=read(release/'build-provenance.json');snapshot=read(work/'source-snapshot/source-manifest.json')
assert snapshot['buildInputs']==proof['source']
for name,h in proof['source'].items():assert hashlib.sha256((dest/'source-snapshot'/name).read_bytes()).hexdigest()==h,name
for archive_name in ('before-replay-generator-fix','before-lua-proxy-fix','before-telemetry-storage-fix'):
 snapshot_receipt=read(dest/archive_name/'source-snapshot/source-manifest.json')
 archived_proof=read(dest/archive_name/'release/build-provenance.json')
 assert snapshot_receipt['buildInputs']==archived_proof['source']
 for name,h in archived_proof['source'].items():assert hashlib.sha256((dest/archive_name/'source-snapshot'/name).read_bytes()).hexdigest()==h,name
counts=read(work/'documents-closed.json')['tests'];coverage=read(work/'coverage/coverage.json')
manifest={'schemaVersion':1,'phase':'P3','status':'passed','sourceItems':[f'A{i}' for i in range(1,7)]+[f'B{i}' for i in range(1,9)]+[f'C{i}' for i in range(1,5)],
 'parentCommit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root).decode().strip(),'commitTitle':'feat(p3): 完成引擎复用与独立交付验收',
 'referenceCommit':'4ba5b7cd106c282e7ed166ff853aeea87b392680','tests':counts,'adoption':'engine-delivery-verified-with-explicit-historical-byte-limits',
 'files':hashes,'rawEvidenceEncodings':encodings,'implementationInputs':{'Project/AzureRender/'+n:h for n,h in proof['source'].items()},
 'sourceSnapshot':{'path':'source-snapshot','manifest':'source-snapshot/source-manifest.json','policy':'exact raw bytes; copied into committed evidence'},
 'historicalByteAudit':coverage['historicalByteAudit'],'packages':read(work/'delivery-packages.json'),
 'longRun':read(work/'long-run/summary.json'),'inputEvidence':{'physicalKeyboardMouse':'not performed','host':'production operations and input replay'}}
(dest/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
for n,h in hashes.items():assert hashlib.sha256((dest/n).read_bytes()).hexdigest()==h,n
print(json.dumps({'status':'passed','tests':counts,'reports':len(hashes),'exactProductionInputs':len(proof['source']),'historicalByteLimits':len(coverage['historicalByteAudit']['unverified'])}),flush=True)
