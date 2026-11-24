"""Prove real Vulkan validation-layer insertion in Release procedural workflows."""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
root=Path(__file__).resolve().parents[3]
work=root/'build/evolution/r8'
drivers=json.loads((root/'docs/acceptance/p1/evidence/2026-10-05/driver-inventory.json').read_text())
driver=next(row for row in drivers if row['DriverDesc']=='Intel(R) UHD Graphics')
assert hashlib.sha256(Path(driver['VulkanDriverName']).read_bytes()).hexdigest()==driver['manifestSha256']
cases=[('intel',root/'build/ninja-msvc-release/AzureRender.exe'),
       ('installed',root/'build/ninja-msvc-release/release-gate/install-moved/bin/AzureRender.exe')]
rows=[]
for name,executable in cases:
 env=dict(os.environ,VK_INSTANCE_LAYERS='VK_LAYER_KHRONOS_validation',VK_LAYER_VALIDATE_SYNC='1',VK_LOADER_DEBUG='error,layer')
 if name=='intel':env.update(VK_DRIVER_FILES=driver['VulkanDriverName'],VK_ICD_FILENAMES=driver['VulkanDriverName'])
 output=work/('forced-validation-'+name)
 with (work/('forced-validation-'+name+'.log')).open('w',encoding='utf-8') as log:
  result=subprocess.run(['python','tools/test_procedural_host.py','--executable',str(executable),'--output',str(output)],cwd=root,env=env,stdout=log,stderr=subprocess.STDOUT)
 assert result.returncode==0,(work/('forced-validation-'+name+'.log')).read_text(encoding='utf-8')[-5000:]
 run=sorted(output.glob('run-*'))[-1]
 for workflow in ('exploration','inspector'):
  logs='\n'.join((run/workflow/file).read_text(encoding='utf-8') for file in ('stdout.log','stderr.log'))
  assert not any(marker in logs.lower() for marker in ('vuid-','validation error','sync-hazard','hazard detected','failed to load layer')),logs[-10000:]
  assert re.search(r'Insert.*VK_LAYER_KHRONOS_validation',logs,re.I),logs[-10000:]
 rows.append(dict(device=name,status='passed',layerInserted=True,synchronizationValidationRequested=True,workflows=2))
 print(name+' forced validation passed',flush=True)
(work/'forced-validation.json').write_text(json.dumps(dict(status='passed',cases=rows),indent=2),encoding='utf-8')
