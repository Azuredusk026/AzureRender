"""Collect only current R9 results and verify source, product and experiment gates."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
root=Path(__file__).resolve().parents[3]
work=root/'build/evolution/r9'
build=root/'build/ninja-msvc-release'
destination=root/'docs/acceptance/r9/evidence/2026-10-07'
destination.mkdir(parents=True,exist_ok=True)
reports={}
for path in work.iterdir():
 if path.is_file() and path.suffix in {'.log','.json','.py'}:reports[path.name]=path
reports.update({'release-tests.log':build/'release-gate/test.log','release-result.json':build/'release-gate/result.json',
 'debug-provenance.json':root/'build/ninja-msvc-debug/build-provenance.json','release-provenance.json':build/'build-provenance.json'})
for folder in ['performance','scale-rtx','scale-intel','visibility-rtx','visibility-intel']:
 for path in (work/folder).rglob('*'):
  if path.is_file() and path.suffix in {'.log','.json','.csv','.png'}:
   reports[path.relative_to(work).as_posix()]=path
for archived in ('before-scheduling-fix','visibility-host-red'):
 for path in (work/archived).rglob('*'):
  if path.is_file() and path.suffix in {'.log','.json','.csv'}:
   reports[path.relative_to(work).as_posix()]=path
for folder in ['intel','inspector-package','generated-package','intel-procedural','installed-procedural']:
 reports[folder+'/summary.json']=work/folder/'summary.json'
for folder in ['forced-validation-intel','forced-validation-installed']:
 run=sorted((work/folder).glob('run-*'))[-1]
 for workflow in ['exploration','inspector']:
  for name in ['stdout.log','stderr.log','runtime.json','result.json','control-results.json']:
   reports[folder+'/'+workflow+'/'+name]=run/workflow/name
for name in ['visual-regression-summary.json','visual-regression-legacy-summary.json','visual-regression-legacy-skinning-summary.json',
 'playable-package/evidence/summary.json','playable-package/evidence/cycles.summary.json']:
 reports['regression/'+name.replace('/','-')]=build/name
counts={}
for config,path in [('Debug',work/'debug-tests.log'),('Release',build/'release-gate/test.log')]:
 match=re.search(r'100% tests passed, 0 tests failed out of (\d+)',path.read_text(encoding='utf-8',errors='replace'))
 assert match,config+' full test gate failed';counts[config]=int(match[1])
assert json.loads((work/'performance/strict-budget.json').read_text())['passed']
assert json.loads((work/'post-gates-status.json').read_text())['status']=='passed'
for device in ('rtx','intel'):
 assert 'GPU independent joints, exact batched equivalence and output isolation passed' in (work/('skinning-'+device+'.log')).read_text()
assert json.loads((work/'modules-off.json').read_text())['status']=='passed'
for name in ['scale-rtx','scale-intel','visibility-rtx','visibility-intel']:
 assert json.loads((work/name/'summary.json').read_text())['status']=='passed'
assert json.loads((work/'forced-validation.json').read_text())['status']=='passed'
hashes={}
for name,path in reports.items():
 assert path.is_file(),str(path)
 target=destination/name;target.parent.mkdir(parents=True,exist_ok=True)
 data=path.read_bytes()
 if path.suffix=='.log' and (data.startswith(b'\xff\xfe') or b'\x00' in data[:100]):data=data.decode('utf-16').encode('utf-8')
 target.write_bytes(data);hashes[name]=hashlib.sha256(data).hexdigest()
repository=root.parent.parent
changed=subprocess.check_output(['git','diff','--name-only','HEAD'],cwd=repository,text=True).splitlines()
untracked=subprocess.check_output(['git','ls-files','--others','--exclude-standard'],cwd=repository,text=True).splitlines()
sources={}
for name in sorted(set(changed+untracked)):
 path=repository/name
 if path.is_file() and '/docs/acceptance/' not in name and path.suffix in {'.cpp','.hpp','.py','.txt','.json','.comp','.frag','.vert','.cmake'}:
  sources[name]=hashlib.sha256(path.read_bytes()).hexdigest()
manifest=dict(schemaVersion=1,phase='R9',status='passed',sourceItems=['C3'],tests=counts,
 parentCommit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),
 referenceCommit='4ba5b7cd106c282e7ed166ff853aeea87b392680',adoption='verified-prototype-retained',
 parts={'resourceContract':'formal-adoption','visibility':'verified-prototype-retained'},files=hashes,implementationInputs=sources,
 inputEvidence={'physicalKeyboardMouse':'not performed','host':'production rendering, independent GPU readback and input replay'},
 limits={'platform':'Windows','defaultRenderer':'existing GPU frustum path',
 'prototype':'stable command and surface identities; CPU visibility retained; no Hi-Z or visibility-buffer shading',
 'resourceUpdates':'single owner thread; caller completes GPU work before destruction; frame-retired rebuild'})
(destination/'.gitattributes').write_text('* -text -whitespace\n',encoding='utf-8')
(destination/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
print(json.dumps(dict(status='passed',tests=counts,reports=len(hashes),sources=len(sources))))
