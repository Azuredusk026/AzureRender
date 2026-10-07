"""Run G11 device, budget and delivery gates against frozen products."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3];work=root/'build/evolution/g11'
release=root/'build/ninja-msvc-release';editor=release/'AzureRender.exe'
def run(name,args,env=None):
    with (work/(name+'.log')).open('w',encoding='utf-8') as log:
        result=subprocess.run(args,cwd=root,env=env,stdout=log,stderr=subprocess.STDOUT)
    if result.returncode:raise RuntimeError(name+': '+(work/(name+'.log')).read_text(encoding='utf-8',errors='replace')[-6500:])
    print(name+' passed',flush=True)
assert json.loads((release/'release-gate/result.json').read_text(encoding='utf-8'))['status']=='passed'
drivers=json.loads((root/'docs/acceptance/p1/evidence/2026-10-05/driver-inventory.json').read_text(encoding='utf-8'))
driver=next(row for row in drivers if row['DriverDesc']=='Intel(R) UHD Graphics')
assert hashlib.sha256(Path(driver['VulkanDriverName']).read_bytes()).hexdigest()==driver['manifestSha256']
env=dict(os.environ,VK_DRIVER_FILES=driver['VulkanDriverName'],VK_ICD_FILENAMES=driver['VulkanDriverName'],VK_LAYER_VALIDATE_SYNC='1')
run('skinning-rtx',[str(release/'AzureSkinningGpuTests.exe'),str(release/'shaders/skin.comp.spv')])
run('skinning-intel',[str(release/'AzureSkinningGpuTests.exe'),str(release/'shaders/skin.comp.spv')],env)
for backend in ('coreclr','nativeaot'):
    run('managed-intel-'+backend,['python','tools/test_managed_workflows.py','--build-dir',str(release),
        '--backend',backend,'--configuration','Release','--evidence',str(work/('managed-intel-'+backend))],env)
run('performance',['python',str(work/'measure_performance.py')])
run('intel-procedural',['python','tools/test_procedural_host.py','--executable',str(editor),'--output',str(work/'intel-procedural')],env)
run('intel',['python',str(work/'run_intel.py')])
run('inspector-package',['python',str(work/'run_inspector_package.py')])
run('generated-package',['python',str(work/'run_generated_package.py')])
install=release/'release-gate/install-moved'
run('installed-procedural',['python','tools/test_procedural_host.py','--executable',str(install/'bin/AzureRender.exe'),'--output',str(work/'installed-procedural')])
run('forced-validation',['python',str(work/'run_sync_validation.py')])
run('module-boundaries',['python','tools/check_module_boundaries.py','--source','.','--build-dir',str(release),'--output',str(work/'module-boundaries.json')])
for config in ('debug','release'):
    run(config+'-provenance',['python','tools/build_provenance.py','verify','--build-dir','build/ninja-msvc-'+config,'--config',config.title()])
(work/'post-gates-status.json').write_text(json.dumps({'status':'passed'},indent=2)+'\n')
print('G11 frozen device, budget and delivery gates passed',flush=True)
