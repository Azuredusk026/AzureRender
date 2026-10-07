"""Archive G11 raw results only after every required gate passes."""
import hashlib
import json
from pathlib import Path
import re
import subprocess

root=Path(__file__).resolve().parents[3]
work=root/'build/evolution/g11'
release=root/'build/ninja-msvc-release'
destination=root/'docs/acceptance/g11/evidence/2026-10-07'
def read(path):return json.loads(path.read_text(encoding='utf-8-sig'))
assert read(work/'post-gates-status.json')['status']=='passed'
assert read(work/'modules-off.json')['status']=='passed'
assert read(work/'performance/strict-budget.json')['passed']
assert read(work/'forced-validation.json')['status']=='passed'
assert read(release/'release-gate/result.json')['status']=='passed'
counts={}
for config,path in [('Debug',work/'debug-tests.log'),('Release',release/'release-gate/test.log')]:
    match=re.search(r'100% tests passed, 0 tests failed out of (\d+)',path.read_text(encoding='utf-8'))
    assert match,config;counts[config]=int(match[1])
reports={p.name:p for p in work.iterdir() if p.is_file() and p.suffix in {'.log','.json','.py'}}
reports.update({'release-tests.log':release/'release-gate/test.log',
    'release-last-test.log':release/'Testing/Temporary/LastTest.log',
    'release-result.json':release/'release-gate/result.json',
    'managed-build-manifest.json':release/'managed/manifest.json',
    'debug-provenance.json':root/'build/ninja-msvc-debug/build-provenance.json',
    'release-provenance.json':release/'build-provenance.json'})
for folder in ['performance','intel','inspector-package','generated-package','intel-procedural','installed-procedural',
               'managed-intel-coreclr','managed-intel-nativeaot','forced-validation-intel','forced-validation-installed',
               'before-runtime-trace-fix','trace-red-retained','trace-green']:
    for p in (work/folder).rglob('*'):
        if p.is_file() and p.suffix in {'.log','.json','.csv'} and not any(x in p.parts for x in ('game','share','assets','bin')):
            reports[p.relative_to(work).as_posix()]=p
for config in ['debug','release']:
    base=root/('build/ninja-msvc-'+config)/'managed-workflows'
    for p in base.rglob('*'):
        if p.is_file() and p.suffix in {'.log','.json','.csv'} and not any(x in p.parts for x in ('game','share','assets','bin')):
            reports['workflows/'+config+'/'+p.relative_to(base).as_posix()]=p
for name in ['visual-regression-summary.json','visual-regression-legacy-summary.json',
             'visual-regression-legacy-skinning-summary.json','playable-package/evidence/summary.json',
             'playable-package/evidence/cycles.summary.json']:
    reports['regression/'+name.replace('/','-')]=release/name
hashes={};destination.mkdir(parents=True,exist_ok=True)
for name,path in reports.items():
    assert path.is_file(),path
    data=path.read_bytes()
    if data.startswith(b'\xff\xfe'):data=data.decode('utf-16').encode('utf-8')
    target=destination/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
    hashes[name]=hashlib.sha256(data).hexdigest()
repository=root.parent.parent
names=subprocess.check_output(['git','diff','--name-only','HEAD'],cwd=repository,text=True).splitlines()
names+=subprocess.check_output(['git','ls-files','--others','--exclude-standard'],cwd=repository,text=True).splitlines()
sources={}
for name in sorted(set(names)):
    path=repository/name
    if path.is_file() and '/docs/acceptance/' not in name and path.suffix in {'.cpp','.hpp','.py','.cs','.csproj','.json','.txt','.cmake'}:
        sources[name]=hashlib.sha256(path.read_bytes()).hexdigest()
manifest=dict(schemaVersion=1,phase='G11',status='passed',sourceItems=['C4'],tests=counts,
    parentCommit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),
    referenceCommit='4ba5b7cd106c282e7ed166ff853aeea87b392680',adoption='verified-prototype-retained',
    parts={'scriptService':'formal-adoption','generatedBindings':'formal-adoption','managedBackends':'verified-prototype-retained'},
    files=hashes,implementationInputs=sources,inputEvidence={'physicalKeyboardMouse':'not performed','host':'production operation and input replay'},
    limits={'platform':'Windows x64','defaultBackend':'lua','managedContent':'trusted compiled extension',
            'coreclr':'installed .NET 9.0.11; single user assembly plus Engine API; collectible script context',
            'nativeaot':'rebuild module and restart; module mapping retained until process exit'})
(destination/'.gitattributes').write_text('* -text -whitespace\n',encoding='utf-8')
(destination/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'status':'passed','tests':counts,'reports':len(hashes),'sources':len(sources)}))
