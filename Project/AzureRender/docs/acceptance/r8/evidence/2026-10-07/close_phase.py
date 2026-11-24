"""Finalize package and source evidence after all R8 gates pass."""
import hashlib
import json
from pathlib import Path
import subprocess
import tarfile
root=Path(__file__).resolve().parents[3]
work=root/'build/evolution/r8'
evidence=root/'docs/acceptance/r8/evidence/2026-10-07'
def run(name,args):
 with (work/(name+'.log')).open('w',encoding='utf-8') as output:
  result=subprocess.run(args,cwd=root,stdout=output,stderr=subprocess.STDOUT)
 if result.returncode:raise RuntimeError(name+': '+(work/(name+'.log')).read_text(encoding='utf-8',errors='replace')[-6000:])
 print(name+' passed',flush=True)
run('document-sync',['python',str(work/'sync_documents.py')])
evidence.mkdir(parents=True,exist_ok=True)
(evidence/'manifest.json').write_text(json.dumps(dict(phase='R8',status='collecting')),encoding='utf-8')
run('docs-check-final',['D:/Git/Git/bin/bash.exe','tools/check_docs.sh'])
run('acceptance-style-final',['python','tools/check_doc_style.py','docs/acceptance/r8/2026-10-07.md',
 'docs/runtime/shader-modules.md','docs/tutorials/shader-modules.md'])
run('repository-audit',['python','tools/audit_repository.py','--source','../..','--output',str(work/'repository-audit.json')])
run('package-finalize',[str(root/'tools/msvc_env.bat'),'python',str(work/'finalize_package.py')])
install=root/'build/ninja-msvc-release/release-gate/install-moved'
for name in ['docs/runtime/shader-modules.md','docs/tutorials/shader-modules.md']:
 assert (install/'share/AzureRender'/name).read_bytes()==(root/name).read_bytes(),name
for name in ['bloom-direct.spv','bloom-cached.spv']:
 source=root/'build/ninja-msvc-release/shader-modules'/name
 assert (install/'share/AzureRender/shaders/modules'/name).read_bytes()==source.read_bytes(),name
package=json.loads((work/'final-package-result.json').read_text())
assert hashlib.sha256(Path(package['package']).read_bytes()).hexdigest()==package['sha256']
with tarfile.open(package['package'],'r:gz') as archive:
 for relative in ['docs/runtime/shader-modules.md','docs/tutorials/shader-modules.md',
  'shaders/modules/bloom-direct.spv','shaders/modules/bloom-cached.spv']:
  member=next(item for item in archive.getmembers() if item.name.endswith('/share/AzureRender/'+relative))
  assert archive.extractfile(member).read()==(install/'share/AzureRender'/relative).read_bytes(),relative
# The collector includes this strict-build log; seed it for the first pass.
(work/'site-final.log').write_text('Strict site build pending evidence collection\n',encoding='utf-8')
run('evidence-collect',['python',str(work/'collect_evidence.py')])
run('site-final',['python','-m','mkdocs','build','--strict','--site-dir',str(work/'site-final')])
run('evidence-collect',['python',str(work/'collect_evidence.py')])
manifest=json.loads((evidence/'manifest.json').read_text())
assert manifest['status']=='passed'
for name,expected in manifest['files'].items():assert hashlib.sha256((evidence/name).read_bytes()).hexdigest()==expected,name
for name,expected in manifest['implementationInputs'].items():assert hashlib.sha256((root.parent.parent/name).read_bytes()).hexdigest()==expected,name
for config in ['debug','release']:
 run(config+'-provenance-close',['python','tools/build_provenance.py','verify','--build-dir','build/ninja-msvc-'+config,'--config',config.title()])
print('R8 source, product, documentation, package and raw evidence hashes verified',flush=True)
