"""Run G10 device and delivery checks serially after full regression."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
work=root/'build/evolution/g10'
executable=root/'build/ninja-msvc-release/AzureRender.exe'
def run(name,args,env=None):
 with (work/(name+'.log')).open('w',encoding='utf-8') as log:
  result=subprocess.run(args,cwd=root,env=env,stdout=log,stderr=subprocess.STDOUT)
 if result.returncode:raise RuntimeError(name+': '+(work/(name+'.log')).read_text(encoding='utf-8',errors='replace')[-6000:])
 print(name+' passed',flush=True)
assert json.loads((root/'build/ninja-msvc-release/release-gate/result.json').read_text())['status']=='passed'
run('performance',['python',str(work/'measure_performance.py')])
drivers=json.loads((root/'docs/acceptance/p1/evidence/2026-10-05/driver-inventory.json').read_text())
driver=next(row for row in drivers if row['DriverDesc']=='Intel(R) UHD Graphics')
assert hashlib.sha256(Path(driver['VulkanDriverName']).read_bytes()).hexdigest()==driver['manifestSha256']
env=dict(os.environ,VK_DRIVER_FILES=driver['VulkanDriverName'],VK_ICD_FILENAMES=driver['VulkanDriverName'],VK_LAYER_VALIDATE_SYNC='1')
run('intel-procedural',['python','tools/test_procedural_host.py','--executable',str(executable),'--output',str(work/'intel-procedural')],env)
run('intel',['python',str(work/'run_intel.py')])
run('inspector-package',['python',str(work/'run_inspector_package.py')])
run('generated-package',['python',str(work/'run_generated_package.py')])
install=root/'build/ninja-msvc-release/release-gate/install-moved'
run('installed-procedural',['python','tools/test_procedural_host.py','--executable',str(install/'bin/AzureRender.exe'),'--output',str(work/'installed-procedural')])
run('module-boundaries',['python','tools/check_module_boundaries.py','--source','.','--build-dir','build/ninja-msvc-release','--output',str(work/'module-boundaries.json')])
for config in ('debug','release'):
 run(config+'-provenance',['python','tools/build_provenance.py','verify','--build-dir','build/ninja-msvc-'+config,'--config',config.title()])
