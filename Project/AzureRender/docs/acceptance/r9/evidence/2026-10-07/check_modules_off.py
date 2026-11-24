"""Build the production hosts with optional Slang compiler unavailable."""
import hashlib
import json
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
work=root/'build/evolution/r9'
build=root/'build/ninja-msvc-release'
products=[build/(name+'.exe') for name in ['AzureRender','AzurePlayer']]
before={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in products}
def run(name,args):
 with (work/(name+'.log')).open('w',encoding='utf-8') as output:
  result=subprocess.run([str(root/'tools/msvc_env.bat'),*args],cwd=root,stdout=output,stderr=subprocess.STDOUT)
 if result.returncode: raise RuntimeError(name+' failed; inspect log')
try:
 run('modules-off-configure',['cmake','-S','.', '-B',str(build),'-DAZURE_ENABLE_SHADER_MODULE_PROTOTYPE=OFF',
  '-DAZURE_SLANG_COMPILER='+str(work/'unavailable-slangc.exe')])
 run('modules-off-build',['cmake','--build',str(build),'--target','AzureRender','AzurePlayer'])
 discovery=subprocess.check_output(['ctest','--test-dir',str(build),'-N','--show-only=json-v1'],cwd=root,text=True)
 tests=json.loads(discovery)['tests']
 assert not any('ShaderModule' in t['name'] or 'ShaderComposition' in t['name'] for t in tests)
 after={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in products}
 assert after==before,'Optional prototype must not alter production host binaries'
 (work/'modules-off.json').write_text(json.dumps(dict(status='passed',compilerUnavailable=True,
  prototypeTestsAbsent=True,productionHostsIdentical=True,sha256=after),indent=2),encoding='utf-8')
finally:
 run('modules-on-restore',['cmake','-S','.', '-B',str(build),'-DAZURE_ENABLE_SHADER_MODULE_PROTOTYPE=ON',
  '-DAZURE_SLANG_COMPILER=C:/VulkanSDK/1.4.350.0/Bin/slangc.exe'])
