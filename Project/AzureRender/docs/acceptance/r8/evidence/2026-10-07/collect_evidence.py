"""Collect R8 raw gate results, implementation identities and module artifacts."""
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
root=Path(__file__).resolve().parents[3]
work=root/'build/evolution/r8'
build=root/'build/ninja-msvc-release'
destination=root/'docs/acceptance/r8/evidence/2026-10-07'
destination.mkdir(parents=True,exist_ok=True)
reports={}
for name in ['debug-build.log','debug-tests.log','release-build.log','release-gate.log',
 'publication-red.log','publication-green.log','compiler-library-red.log','compiler-library-green.log','slang-compiler-license.txt','shader-types-red.log','shared-host-red.log',
 'composition-red.log','strategy-diagnostic-red.log','cache-reflection-red.log','schema-version-red.log',
 'module-boundaries.json','forced-validation.json','final-package-result.json','final-install-verify.log',
 'docs-check-final.log','site-final.log','repository-audit.json','run_gates.py','post_gates.py',
 'measure_performance.py','working_set_budget.py','run_sync_validation.py','collect_evidence.py','check_modules_off.py',
 'modules-off-configure.log','modules-off-build.log','modules-on-restore.log','close_phase.py','sync_documents.py','finalize_package.py']:
 reports[name]=work/name
reports.update({'release-tests.log':build/'release-gate/test.log',
 'release-result.json':build/'release-gate/result.json',
 'debug-provenance.json':root/'build/ninja-msvc-debug/build-provenance.json',
 'release-provenance.json':build/'build-provenance.json'})
for folder in ['performance','compilation-final','scene-rtx','scene-intel']:
 for path in (work/folder).rglob('*'):
  if path.is_file() and path.suffix in {'.log','.json','.csv','.png','.py'} and 'shaders' not in path.parts:
   reports[path.relative_to(work).as_posix()]=path
for folder in ['intel','inspector-package','generated-package','intel-procedural','installed-procedural']:
 reports[folder+'/summary.json']=work/folder/'summary.json'
for folder in ['forced-validation-intel','forced-validation-installed']:
 run=sorted((work/folder).glob('run-*'))[-1]
 for workflow in ['exploration','inspector']:
  for name in ['stdout.log','stderr.log','runtime.json','result.json','control-results.json']:
   reports[folder+'/'+workflow+'/'+name]=run/workflow/name
for name in ['rtx-final-probe.json','intel-final-probe.json','modules-off.json']:
 reports[name]=work/name
for name in ['bloom-direct.spv','bloom-direct.spv.json','bloom-cached.spv','bloom-cached.spv.json']:
 reports['modules/'+name]=build/'shader-modules'/name
for name in ['visual-regression-summary.json','visual-regression-legacy-summary.json',
 'visual-regression-legacy-skinning-summary.json','playable-package/evidence/summary.json',
 'playable-package/evidence/cycles.summary.json']:
 reports['regression/'+name.replace('/','-')]=build/name
counts={}
for config,path in [('Debug',work/'debug-tests.log'),('Release',build/'release-gate/test.log')]:
 text=path.read_text(encoding='utf-8',errors='replace')
 match=re.search(r'100% tests passed, 0 tests failed out of (\d+)',text)
 assert match,config+' full test gate failed'
 counts[config]=int(match[1])
assert json.loads((work/'performance/strict-budget.json').read_text())['passed']
assert json.loads((work/'modules-off.json').read_text())['status']=='passed'
hashes={}
for name,path in reports.items():
 assert path.is_file(),str(path)
 target=destination/name;target.parent.mkdir(parents=True,exist_ok=True)
 data=path.read_bytes()
 if path.suffix=='.log' and (data.startswith(b'\xff\xfe') or b'\x00' in data[:100]):
  data=data.decode('utf-16').encode('utf-8')
 target.write_bytes(data);hashes[name]=hashlib.sha256(data).hexdigest()
repository=root.parent.parent
changed=subprocess.check_output(['git','diff','--name-only','HEAD'],cwd=repository,text=True).splitlines()
untracked=subprocess.check_output(['git','ls-files','--others','--exclude-standard'],cwd=repository,text=True).splitlines()
sources={}
for name in sorted(set(changed+untracked)):
 path=repository/name
 if path.is_file() and '/docs/acceptance/' not in name and path.suffix in {'.cpp','.hpp','.py','.txt','.json','.slang','.cmake'}:
  sources[name]=hashlib.sha256(path.read_bytes()).hexdigest()
manifest=dict(schemaVersion=1,phase='R8',status='passed',sourceItems=['C2'],tests=counts,
 parentCommit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),
 referenceCommit='4ba5b7cd106c282e7ed166ff853aeea87b392680',adoption='verified-prototype-retained',
 compiler=dict(name='Slang',version='2026.8',distribution='external Vulkan SDK compiler; installed Player consumes SPIR-V'),
 files=hashes,implementationInputs=sources,
 inputEvidence=dict(host='production RenderGraph captures on RTX and Intel',physicalKeyboardMouse='not performed'),
 limits=dict(platform='Windows',layout='float32 and uint32 scalar structs, alignment 4',
  publication='caller serializes readers and writers; exception recovery; rebuild after crash',
  defaultRenderer='GLSL; optional composed Bloom experiment'))
(destination/'.gitattributes').write_text('* -text -whitespace\n',encoding='utf-8')
(destination/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
print(json.dumps(dict(status='passed',tests=counts,reports=len(hashes),sources=len(sources))))
