"""Verify the final moved packages on the registered Intel Vulkan device."""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time

ROOT=Path.cwd().resolve();sys.path.insert(0,str(ROOT/'tools'))
from run_playable_long_run import isolated_environment
from test_playable_player import run
import install_delivery

delivery=max((ROOT/'build/p4/delivery').glob('run-*/summary.json'),key=lambda p:p.stat().st_mtime)
package_root=delivery.parent
OUT=ROOT/'build/p4/intel'/('run-'+str(time.time_ns()));OUT.mkdir(parents=True)
drivers=json.loads((ROOT/'docs/acceptance/p1/evidence/2026-10-05/driver-inventory.json').read_text())
driver=next(r for r in drivers if r['DriverDesc']=='Intel(R) UHD Graphics')
assert hashlib.sha256(Path(driver['VulkanDriverName']).read_bytes()).hexdigest()==driver['manifestSha256']
env=isolated_environment()
env.update(VK_DRIVER_FILES=driver['VulkanDriverName'],VK_ICD_FILENAMES=driver['VulkanDriverName'])
player=package_root/'Moved exploration/bin/AzurePlayer.exe'
project=package_root/'Moved exploration/game/project.azureproject'
result=subprocess.run([str(player),'--project',str(project),'--width','960','--height','540',
    '--fixed-frame-step','--smoke-frames','30','--gpu-timing','--gpu-timing-output',str(OUT/'probe.json')],
    cwd=OUT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=120)
(OUT/'probe.stdout.log').write_text(result.stdout,encoding='utf-8');(OUT/'probe.stderr.log').write_text(result.stderr,encoding='utf-8')
assert result.returncode==0,result.stdout+result.stderr
cap=json.loads((OUT/'captures/gpu_capabilities.json').read_text());assert 'Intel' in cap['device_name']
run(player,OUT/'quest',project=project,env=env,cwd=OUT)
install_delivery.OUT=OUT
inspection=install_delivery.inspector(package_root/'Moved inspector',OUT/'inspector',env)
assert 'Intel' in inspection['device']
validation_env=dict(os.environ,VK_DRIVER_FILES=driver['VulkanDriverName'],VK_ICD_FILENAMES=driver['VulkanDriverName'],
    VK_INSTANCE_LAYERS='VK_LAYER_KHRONOS_validation',VK_LAYER_VALIDATE_SYNC='1',VK_LOADER_DEBUG='error,layer')
with (OUT/'validation.log').open('w',encoding='utf-8') as log:
    test=subprocess.run([sys.executable,'tools/test_procedural_host.py','--executable',str(ROOT/'build/ninja-msvc-release/AzureRender.exe'),
        '--output',str(OUT/'validation')],cwd=ROOT,env=validation_env,stdout=log,stderr=subprocess.STDOUT,timeout=600)
assert test.returncode==0
validation=max((OUT/'validation').glob('run-*'),key=lambda p:p.stat().st_mtime)
for workflow in ('exploration','inspector'):
    logs='\n'.join((validation/workflow/name).read_text(encoding='utf-8') for name in ('stdout.log','stderr.log'))
    assert re.search(r'Insert.*VK_LAYER_KHRONOS_validation',logs,re.I)
    assert not any(s in logs.lower() for s in ('vuid-','validation error','sync-hazard','hazard detected','failed to load layer'))
summary=dict(passed=True,device=cap['device_name'],driver=driver,capabilities=cap,configuration='960x540, full materials, four cascades',
    quest=json.loads((OUT/'quest/summary.json').read_text()),inspector=inspection,validationLayerInserted=True,synchronizationValidationRequested=True)
(OUT/'summary.json').write_text(json.dumps(summary,indent=2),encoding='utf-8');print(json.dumps(summary),flush=True)
