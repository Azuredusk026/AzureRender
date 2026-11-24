"""Archive verified subtasks and failed gates without claiming phase completion."""
import gzip, hashlib, json, shutil, zipfile
from pathlib import Path
ROOT=Path.cwd().resolve();WORK=ROOT/'build/implementation-2026-10-10';OUT=ROOT/'docs/acceptance/p5/evidence'
OUT.mkdir(parents=True,exist_ok=False)
def read(name):return json.loads((WORK/name).read_text(encoding='utf-8'))
def digest(path):
 with path.open('rb') as s:return hashlib.file_digest(s,'sha256').hexdigest()
def copy(source,target):
 target.parent.mkdir(parents=True,exist_ok=True)
 if source.stat().st_size>2*1024**2 and source.suffix!='.gz':
  target=target.with_name(target.name+'.gz')
  with source.open('rb') as inp,target.open('wb') as raw:
   with gzip.GzipFile(filename='',mode='wb',fileobj=raw,mtime=0) as out:shutil.copyfileobj(inp,out)
 else:shutil.copy2(source,target)
cpu=read('p5-final-cpu-summary.json');gpu=read('p5-final-gpu-summary.json');delivery=read('p5-delivery/summary.json');workflow=read('p5-workflows-retry2/summary.json');performance=read('p5-performance/summary.json');retest=read('p5-performance-retest/summary.json');longrun=read('p5-long-run/summary.json')
assert all(x['passed'] for x in (cpu,gpu,delivery,workflow))
assert not performance['passed'] and not retest['passed']
assert longrun['actualSeconds']>=1800 and longrun['status'] in ('passed','failed')
checks=dict(cpu=True,gpu=True,workflows=True,delivery=True,performance=False,longRun=longrun['status']=='passed',physicalInput=False,subjectiveAudio=False)
status='ActivePerformanceAndPhysicalInputPending'
progress=dict(passed=False,stage='P5',status=status,checks=checks,baseCommit='6e864eb',budgetsUnchanged=True)
(WORK/'p5-machine-progress.json').write_text(json.dumps(progress,indent=2)+'\n',encoding='utf-8')
for pattern in ('p5-*.log','p5-*.json','p5-*.py','p5-build-*.cmd'):
 for source in WORK.glob(pattern):copy(source,OUT/('scripts' if source.suffix in ('.py','.cmd') else 'logs' if source.suffix=='.log' else 'summaries')/source.name)
for config in ('debug','release'):
 copy(ROOT/f'build/ninja-msvc-{config}/build-provenance.json',OUT/f'source-products-{config}.json')
 build=ROOT/f'build/ninja-msvc-{config}'
 for directory in build.iterdir():
  if directory.is_dir() and directory.name.startswith(('authoring-','managed-authoring-','viewport-navigation-','productization','native-input')):
   for source in directory.rglob('*'):
    if source.is_file() and source.name in ('summary.json','evidence.json','gpu_capabilities.json'):copy(source,OUT/'specialized'/config/source.relative_to(build))
for folder in ('p5-workflows-retry2','p5-performance','p5-performance-retest','p5-memory-diagnose','p5-heap-diagnose','p5-thread-diagnose','p5-delivery','p5-delivery-missing-docs','p5-long-run'):
 for source in (WORK/folder).rglob('*'):
  if not source.is_file():continue
  relative=source.relative_to(WORK/folder)
  if any(p in ('.azure','config','__pycache__') or p.startswith(('editor-config-','Project input ','Moved ','Unavailable ','Output ')) for p in relative.parts):continue
  if source.name=='endpoint.json' or source.suffix in ('.zip','.etl'):continue
  if folder.startswith(('p5-performance','p5-memory-diagnose')) and len(relative.parts)>1:continue
  if folder=='p5-delivery' and source.name=='runtime.json':continue
  copy(source,OUT/folder/relative)
proof=json.loads((ROOT/'build/ninja-msvc-release/build-provenance.json').read_text(encoding='utf-8'))
archive=OUT/'source-inputs.tar.gz'
import io,tarfile
with archive.open('wb') as raw,gzip.GzipFile(filename='',fileobj=raw,mode='wb',mtime=0) as gz,tarfile.open(fileobj=gz,mode='w') as z:
 for name,expected in proof['source'].items():
  data=(ROOT/name).read_bytes();assert hashlib.sha256(data).hexdigest()==expected
  info=tarfile.TarInfo(name);info.size=len(data);info.mtime=0;z.addfile(info,io.BytesIO(data))
with tarfile.open(archive,'r:gz') as z:
 assert {m.name for m in z.getmembers()}==set(proof['source'])
 assert all(hashlib.sha256(z.extractfile(name).read()).hexdigest()==expected for name,expected in proof['source'].items())
files=[dict(file=p.relative_to(OUT).as_posix(),bytes=p.stat().st_size,sha256=digest(p)) for p in sorted(OUT.rglob('*')) if p.is_file() and p.name!='manifest.json']
manifest=dict(schemaVersion=1,stage='P5',date='2026-10-10',status=status,baseCommit='6e864eb',checks=checks,cpuTests={'Debug':154,'Release':154},gpuTests={'Debug':60,'Release':62},manual={'physicalInput':'pending','subjectiveAudio':'pending'},sourceArchive=dict(file='source-inputs.tar.gz',inputs=len(proof['source']),sha256=digest(archive)),files=files)
(OUT/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
assert all(digest(OUT/r['file'])==r['sha256'] for r in files)
print(json.dumps(dict(archived=True,phaseComplete=False,files=len(files),bytes=sum(f['bytes'] for f in files))))
