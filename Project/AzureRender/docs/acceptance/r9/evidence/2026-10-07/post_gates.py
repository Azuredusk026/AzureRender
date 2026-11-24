"""Run R9 device, scale, budget and delivery gates serially."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
work=root/'build/evolution/r9'
executable=root/'build/ninja-msvc-release/AzureRender.exe'
defer_intel='--defer-intel-scale' in sys.argv
resume_scale='--resume-after-scale-rtx' in sys.argv
def run(name,args,env=None):
 with (work/(name+'.log')).open('w',encoding='utf-8') as log:
  result=subprocess.run(args,cwd=root,env=env,stdout=log,stderr=subprocess.STDOUT)
 if result.returncode:raise RuntimeError(name+': '+(work/(name+'.log')).read_text(encoding='utf-8',errors='replace')[-6500:])
 print(name+' passed',flush=True)
assert json.loads((root/'build/ninja-msvc-release/release-gate/result.json').read_text())['status']=='passed'
drivers=json.loads((root/'docs/acceptance/p1/evidence/2026-10-05/driver-inventory.json').read_text())
driver=next(row for row in drivers if row['DriverDesc']=='Intel(R) UHD Graphics')
assert hashlib.sha256(Path(driver['VulkanDriverName']).read_bytes()).hexdigest()==driver['manifestSha256']
env=dict(os.environ,VK_DRIVER_FILES=driver['VulkanDriverName'],VK_ICD_FILENAMES=driver['VulkanDriverName'],VK_LAYER_VALIDATE_SYNC='1')
if resume_scale:
 assert json.loads((work/'modules-off.json').read_text())['status']=='passed'
 assert json.loads((work/'scale-rtx/summary.json').read_text())['status']=='passed'
else:
 run('modules-off',['python',str(work/'check_modules_off.py')])
run('skinning-rtx',[str(root/'build/ninja-msvc-release/AzureSkinningGpuTests.exe'),str(root/'build/ninja-msvc-release/shaders/skin.comp.spv')])
run('skinning-intel',[str(root/'build/ninja-msvc-release/AzureSkinningGpuTests.exe'),str(root/'build/ninja-msvc-release/shaders/skin.comp.spv')],env)
for name,options in [('rtx',[]),('intel',['--driver',driver['VulkanDriverName']])]:
 if name=='rtx' and resume_scale:continue
 if name=='intel' and defer_intel:continue
 run('scale-'+name,['python','tools/run_visibility_scale_benchmark.py','--output',str(work/('scale-'+name)),*options])
for device in ['rtx','intel']:
 if device=='intel' and defer_intel:continue
 data=json.loads((work/('scale-'+device)/'summary.json').read_text())
 for row in data['runs']:
  counters=row['submission']
  assert counters['frames']==150 and counters['instances']==row['instances']*150,'Rendered scale differs from requested scale'
  assert row['skinnedVertices']>0 and row['transparentTriangles']>0,'Experimental mesh lost skinned or transparent complexity'
 print(device+' actual render scale and scene complexity verified',flush=True)
run('visibility-rtx',['python',str(work/'check_visibility_variants.py')])
run('visibility-intel',['python',str(work/'check_visibility_variants.py'),'intel'],env)
run('performance',['python',str(work/'measure_performance.py')])
run('intel-procedural',['python','tools/test_procedural_host.py','--executable',str(executable),'--output',str(work/'intel-procedural')],env)
run('intel',['python',str(work/'run_intel.py')])
run('inspector-package',['python',str(work/'run_inspector_package.py')])
run('generated-package',['python',str(work/'run_generated_package.py')])
install=root/'build/ninja-msvc-release/release-gate/install-moved'
run('installed-procedural',['python','tools/test_procedural_host.py','--executable',str(install/'bin/AzureRender.exe'),'--output',str(work/'installed-procedural')])
run('forced-validation',['python',str(work/'run_sync_validation.py')])
run('module-boundaries',['python','tools/check_module_boundaries.py','--source','.','--build-dir','build/ninja-msvc-release','--output',str(work/'module-boundaries.json')])
for config in ('debug','release'):
 run(config+'-provenance',['python','tools/build_provenance.py','verify','--build-dir','build/ninja-msvc-'+config,'--config',config.title()])
status=dict(status='partial' if defer_intel else 'passed',pending=['Intel scale performance'] if defer_intel else [])
(work/'post-gates-status.json').write_text(json.dumps(status,indent=2)+'\n')
print(json.dumps(status),flush=True)
