"""Archive actual P5 evidence with reproducible hashes and explicit manual gaps."""
from pathlib import Path
import gzip
import hashlib
import json
import shutil

ROOT=Path.cwd().resolve()
WORK=ROOT/'build/implementation-2026-10-10'
OUT=ROOT/'docs/acceptance/p5/evidence'
OUT.mkdir(parents=True,exist_ok=False)

def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream,'sha256').hexdigest()

def copy(source,target,compress=False):
    target.parent.mkdir(parents=True,exist_ok=True)
    if compress:
        target=target.with_name(target.name+'.gz')
        with source.open('rb') as inp,target.open('wb') as raw:
            with gzip.GzipFile(filename='',mode='wb',fileobj=raw,mtime=0) as out:
                shutil.copyfileobj(inp,out)
    else:
        shutil.copy2(source,target)

pipeline=json.loads((WORK/'p5-pipeline-summary.json').read_text(encoding='utf-8'))
assert pipeline['passed'] and len(pipeline['checks'])==4
for source in WORK.glob('p5-*.log'):
    copy(source,OUT/'logs'/source.name)
for name in ('p5-final-cpu-summary.json','p5-final-gpu-summary.json','p5-pipeline-summary.json','p5-regression-source-debug.json','p5-regression-source-release.json'):
    copy(WORK/name,OUT/name)
for name in ('p5-workflows.py','p5-regression.py','p5-performance.py','p5-delivery.py','p5-pipeline.py','p5-collect.py','p5-refresh-install.py','p5-release-final.py','p5-resume-release.py','p5-update-docs.py','p5-refresh-doc-delivery.py','p5-build-debug.cmd','p5-build-release.cmd','run-logged.py'):
    copy(WORK/name,OUT/'scripts'/name)
for config in ('debug','release'):
    copy(ROOT/f'build/ninja-msvc-{config}/build-provenance.json',OUT/f'source-products-{config}.json')
    build=ROOT/f'build/ninja-msvc-{config}'
    for directory in build.iterdir():
        if directory.is_dir() and directory.name.startswith(('authoring-','managed-authoring-','viewport-navigation-','productization','native-input')):
            for source in directory.rglob('*'):
                if source.is_file() and source.name in ('summary.json','evidence.json','gpu_capabilities.json'):
                    copy(source,OUT/'specialized'/config/source.relative_to(build),source.stat().st_size>2*1024**2)
for name in ('src/editor/projects/ProjectOpenService.cpp','src/editor/EditorProjectEditing.cpp',
             'src/editor/EditorWorkspaceUI.cpp','src/editor/panels/SettingsPanel.cpp',
             'tests/EditorProjectFlowTests.cpp','tools/test_editor_productization.py','tools/test_settings_host.py','tools/test_editor_playable.py'):
    copy(ROOT/name,OUT/'source-snapshot'/name)
for source in (WORK/'p5-workflows-retry2').rglob('*'):
    if not source.is_file():continue
    relative=source.relative_to(WORK/'p5-workflows-retry2')
    if any(p in ('.azure','config','__pycache__') for p in relative.parts) or source.name=='endpoint.json':continue
    copy(source,OUT/'workflows'/relative,source.stat().st_size>2*1024**2)
for source in (WORK/'p5-performance').rglob('*'):
    if not source.is_file():continue
    relative=source.relative_to(WORK/'p5-performance')
    if '.azure' in relative.parts:continue
    if len(relative.parts)>1:continue
    copy(source,OUT/'performance'/relative,source.stat().st_size>2*1024**2)
for source in (WORK/'p5-delivery').rglob('*'):
    if not source.is_file():continue
    relative=source.relative_to(WORK/'p5-delivery')
    if any(p.startswith(('Project input ','Moved ','Unavailable ','Output ')) for p in relative.parts):continue
    if source.suffix=='.zip' or '.azure' in relative.parts:continue
    if source.name=='runtime.json':continue
    copy(source,OUT/'delivery'/relative,source.stat().st_size>2*1024**2 and source.suffix!='.gz')
for source in (WORK/'p5-long-run').rglob('*'):
    if source.is_file():
        relative=source.relative_to(WORK/'p5-long-run')
        copy(source,OUT/'long-run'/relative,source.stat().st_size>2*1024**2)

files=[dict(file=p.relative_to(OUT).as_posix(),bytes=p.stat().st_size,sha256=digest(p))
       for p in sorted(OUT.rglob('*')) if p.is_file() and p.name!='manifest.json']
manifest=dict(schemaVersion=1,stage='P5',date='2026-10-10',status='MachineVerifiedPhysicalInputPending',
    baseCommit='6e864eb',cpuTests={'Debug':154,'Release':154},gpuTests={'Debug':60,'Release':62},
    manual={'physicalInput':'pending','subjectiveAudio':'pending'},files=files)
(OUT/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
assert all(digest(OUT/r['file'])==r['sha256'] for r in files)
print(json.dumps(dict(passed=True,files=len(files),bytes=sum(f['bytes'] for f in files))))
