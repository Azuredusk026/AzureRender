"""Repeat the complete Debug gate for the reviewed final R9 source."""
from pathlib import Path
import shutil
import subprocess
root=Path(__file__).resolve().parents[3]
work=root/'build/evolution/r9'
saved=work/'pre-review-gates';saved.mkdir()
for name in ['debug-build.log','debug-tests.log']:
 shutil.copy2(work/name,saved/name)
for name,args in [('debug-build',[str(root/'tools/msvc_env.bat'),'python','tools/build_provenance.py','build','--build-dir','build/ninja-msvc-debug','--config','Debug']),
 ('debug-tests',['ctest','--test-dir','build/ninja-msvc-debug','-C','Debug','--output-on-failure'])]:
 with (work/(name+'.log')).open('w',encoding='utf-8') as output:
  result=subprocess.run(args,cwd=root,stdout=output,stderr=subprocess.STDOUT)
 if result.returncode:raise RuntimeError(name+': '+(work/(name+'.log')).read_text(encoding='utf-8',errors='replace')[-6500:])
 print(name+' reviewed source passed',flush=True)

import hashlib,json
build=root/'build/ninja-msvc-release'
products=[build/(name+'.exe') for name in ['AzureRender','AzurePlayer','AzureMetaGen','AzureGeometryCompiler']]
before={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in products}
with (work/'release-document-build.log').open('w',encoding='utf-8') as output:
 result=subprocess.run([str(root/'tools/msvc_env.bat'),'python','tools/build_provenance.py','build','--build-dir',str(build),'--config','Release'],cwd=root,stdout=output,stderr=subprocess.STDOUT)
assert result.returncode==0,'Documentation install refresh failed'
after={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in products}
assert before==after,'Documentation installation changed executable products'
(work/'release-document-build.json').write_text(json.dumps(dict(status='passed',executablesIdentical=True,products=after),indent=2),encoding='utf-8')
print('Release documentation install refresh retained all executable hashes',flush=True)
