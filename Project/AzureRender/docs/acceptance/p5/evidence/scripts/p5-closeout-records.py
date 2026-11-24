"""Retain investigated failures and verified subtasks as separate acceptance states."""
import gzip,hashlib,json,shutil
from pathlib import Path
ROOT=Path.cwd().resolve();WORK=ROOT/'build/implementation-2026-10-10'
OUT=ROOT/'docs/acceptance/p5/evidence'
def read(path):return json.loads(path.read_text(encoding='utf-8'))
def digest(path):
    with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()
def copy(source,target):
    target.parent.mkdir(parents=True,exist_ok=True)
    if source.stat().st_size>2*1024**2 and source.suffix!='.gz':
        target=target.with_name(target.name+'.gz')
        with source.open('rb') as inp,target.open('wb') as raw:
            with gzip.GzipFile(filename='',mode='wb',fileobj=raw,mtime=0) as out:shutil.copyfileobj(inp,out)
    else:shutil.copy2(source,target)
control=read(WORK/'p5-capture-layer-control/summary.json')
equivalence=read(WORK/'p5-closeout-source-equivalence.json')
assert equivalence['passed'] and equivalence['changedInputs']==['CMakeLists.txt']
affected=read(WORK/'p5-closeout-tests.json');static=read(WORK/'p5-closeout-static.json')
assert affected['passed'] and static['passed']
assert not control['passed']
assert all(len(rows)==3 for rows in control['rounds'].values())
native=read(WORK/'p5-allocation-debug-native-entry/analysis.json')
resident=read(WORK/'p5-allocation-debug-residency/analysis.json')
assert native['completion']['completed'] and resident['completion']['completed']
assert native['completion']['executableSha256']==resident['completion']['executableSha256']
assert native['completion']['executableSha256']==digest(ROOT/'build/ninja-msvc-release/AzureRender.exe')
investigation=read(WORK/'p5-memory-investigation.json')
investigation['nativeAllocationDiagnostics']=dict(status='diagnosticComplete',
    runs=[dict(directory='p5-allocation-debug-native-entry',requests=native['requestCount']),
          dict(directory='p5-allocation-debug-residency',requests=resident['requestCount'])],
    attribution='unresolved',successfulOwnedProcessExit=True,productionExecutableUnchanged=True,
    limitations=native['limitations'])
investigation['captureLayerControl']=dict(passed=False,diagnosticOnly=True,
    environment=control['environment'],rounds=9,budgetsUnchanged=True,
    originalSourceAssertionFailed=True,sourceEquivalence='p5-closeout-source-equivalence.json',
    conclusion='Capture suppression does not resolve working-set failures',
    failedRounds=[dict(load=name,round=i+1,growth=row['stableWorkingSetGrowth'])
                  for name,rows in control['rounds'].items() for i,row in enumerate(rows) if not row['passed']])
(WORK/'p5-memory-investigation.json').write_text(json.dumps(investigation,indent=2)+'\n',encoding='utf-8')
manifest_path=ROOT/'docs/plans/hazel-fermion-manifest.json'
plan=read(manifest_path)
plan['execution']['productizationEvidence']['allocationDiagnostics']=investigation['nativeAllocationDiagnostics']
plan['execution']['productizationEvidence']['captureLayerControl']=investigation['captureLayerControl']
plan['execution']['productizationEvidence']['verifiedSubtasks']=['authoring','delivery','regression','longRun']
manifest_path.write_text(json.dumps(plan,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
for name in ('p5-allocation-debug','p5-allocation-debug-virtualalloc','p5-allocation-debug-native',
             'p5-allocation-debug-native-entry','p5-allocation-debug-residency','p5-capture-layer-control'):
    for source in (WORK/name).rglob('*'):
        relative=source.relative_to(WORK/name)
        if source.is_file() and not any(part in ('.azure','config','__pycache__') or part.startswith('editor-config-') for part in relative.parts):
            copy(source,OUT/name/relative)
for name in ('p5-allocation-debug.py','p5-analyze-allocation.py','p5-capture-layer-control.py',
             'p5-closeout-records.py','p5-closeout-tests.py','p5-closeout-static.py','p5-resume-stage.py'):
    copy(WORK/name,OUT/'scripts'/name)
for pattern in ('p5-closeout*.log','p5-closeout*.json','p5-capture-layer-control.log','p5-diagnostic-dependency.log','p5-memory-investigation.json'):
    for source in WORK.glob(pattern):copy(source,OUT/('logs' if source.suffix=='.log' else 'summaries')/source.name)
manifest=read(OUT/'manifest.json')
manifest['investigation']=dict(nativeRequests=[native['requestCount'],resident['requestCount']],
    captureLayerControlPassed=False,privateAllocationAttribution='unresolved')
manifest['rawEvidenceStorage']='Git -text and -whitespace; original bytes and SHA-256 retained'
manifest['affectedRegressionRecheck']=dict(passed=True,Debug=5,Release=5,distinctAdditionalTests=0,
    evidence='summaries/p5-closeout-tests.json')
manifest['finalStaticChecks']=dict(passed=True,evidence='summaries/p5-closeout-static.json')
manifest['files']=[dict(file=p.relative_to(OUT).as_posix(),bytes=p.stat().st_size,sha256=digest(p))
    for p in sorted(OUT.rglob('*')) if p.is_file() and p!=OUT/'manifest.json']
(OUT/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
assert not manifest['checks']['performance'] and not manifest['checks']['physicalInput']
assert all(digest(OUT/row['file'])==row['sha256'] for row in manifest['files'])
print(json.dumps(dict(phaseComplete=False,evidenceFiles=len(manifest['files']),
                     captureLayerControl=investigation['captureLayerControl'])),flush=True)
