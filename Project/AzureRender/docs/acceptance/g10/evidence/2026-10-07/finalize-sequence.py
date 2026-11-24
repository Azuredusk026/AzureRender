"""Finalize G10 documentation, licenses, package and verified evidence."""
import hashlib
import json
from pathlib import Path
import subprocess
import tarfile
root=Path(__file__).resolve().parents[3]
work=root/'build/evolution/g10'
evidence=root/'docs/acceptance/g10/evidence/2026-10-07'
def run(name,args):
 with (work/(name+'.log')).open('w',encoding='utf-8') as output:
  result=subprocess.run(args,cwd=root,stdout=output,stderr=subprocess.STDOUT)
 if result.returncode:raise RuntimeError(name+': '+(work/(name+'.log')).read_text(encoding='utf-8',errors='replace')[-6000:])
 print(name+' passed',flush=True)
assert json.loads((work/'performance/strict-budget.json').read_text())['passed']
run('document-sync',['python',str(work/'sync_documents.py')])
evidence.mkdir(parents=True,exist_ok=True)
(evidence/'manifest.json').write_text(json.dumps(dict(phase='G10',status='collecting')),encoding='utf-8')
run('docs-check-final',['D:/Git/Git/bin/bash.exe','tools/check_docs.sh'])
run('acceptance-style-final',['python','tools/check_doc_style.py','docs/acceptance/g10/2026-10-07.md','docs/runtime/procedural-content.md','docs/tutorials/procedural-content.md'])
run('repository-audit',['python','tools/audit_repository.py','--source','../..','--output',str(work/'repository-audit.json')])
run('package-finalize',[str(root/'tools/msvc_env.bat'),'python',str(work/'finalize_package.py')])
install=root/'build/ninja-msvc-release/release-gate/install-moved'
license_hashes={}
for name in ('manifold','clipper2','tbb','hwloc'):
 local=root/'build/ninja-msvc-release/vcpkg_installed/x64-windows/share'/name/'copyright'
 shipped=install/'share/AzureRender/licenses'/(name+'-LICENSE.txt')
 assert local.read_bytes()==shipped.read_bytes(),name
 license_hashes[name]=hashlib.sha256(shipped.read_bytes()).hexdigest()
(work/'procedural-licenses.json').write_text(json.dumps(dict(status='passed',sha256=license_hashes),indent=2),encoding='utf-8')
for relative in ('docs/runtime/procedural-content.md','docs/tutorials/procedural-content.md','docs/acceptance/g10/2026-10-07.md'):
 assert (install/'share/AzureRender'/relative).read_bytes()==(root/relative).read_bytes(),relative
package=json.loads((work/'final-package-result.json').read_text())
assert hashlib.sha256(Path(package['package']).read_bytes()).hexdigest()==package['sha256']
with tarfile.open(package['package'],'r:gz') as archive:
 for relative in ('docs/runtime/procedural-content.md','docs/tutorials/procedural-content.md'):
  member=next(item for item in archive.getmembers() if item.name.endswith('/share/AzureRender/'+relative))
  assert archive.extractfile(member).read()==(root/relative).read_bytes(),relative
run('evidence-collect',['python',str(work/'collect_evidence.py')])
run('site-final',['python','-m','mkdocs','build','--strict','--site-dir',str(work/'site-final')])
run('evidence-collect',['python',str(work/'collect_evidence.py')])
manifest=json.loads((evidence/'manifest.json').read_text())
assert manifest['status']=='passed'
for name,expected in manifest['files'].items():assert hashlib.sha256((evidence/name).read_bytes()).hexdigest()==expected,name
for name,expected in manifest['implementationInputs'].items():assert hashlib.sha256((root.parent.parent/name).read_bytes()).hexdigest()==expected,name
for configuration in ('debug','release'):
 run(configuration+'-provenance-close',['python','tools/build_provenance.py','verify','--build-dir','build/ninja-msvc-'+configuration,'--config',configuration.title()])
print('G10 evidence, licenses, documentation and exact source/product/package hashes verified',flush=True)
