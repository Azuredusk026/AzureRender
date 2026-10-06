"""Run R7 device and delivery checks serially after full release gates."""
import hashlib
import json
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
work = root / 'build/evolution/r7'
executable = root / 'build/ninja-msvc-release/AzureRender.exe'
shader = root / 'build/ninja-msvc-release/shader-reload.json'

def run(name, args, env=None):
    with (work / (name + '.log')).open('w', encoding='utf-8') as log:
        result = subprocess.run(args, cwd=root, env=env, stdout=log, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(name + ': ' + (work / (name + '.log')).read_text(encoding='utf-8', errors='replace')[-6000:])
    print(name + ' passed', flush=True)

assert json.loads((root/'build/ninja-msvc-release/release-gate/result.json').read_text())['status']=='passed'
run('performance', ['python', str(work/'measure_performance.py')])
run('preview-comparison', ['python','tools/run_preview_comparison.py','--executable',str(executable),'--output',str(work/'preview-comparison')])
run('intel', ['python',str(work/'run_intel.py')])
drivers=json.loads((root/'docs/acceptance/p1/evidence/2026-10-05/driver-inventory.json').read_text())
driver=next(row for row in drivers if row['DriverDesc']=='Intel(R) UHD Graphics')
assert hashlib.sha256(Path(driver['VulkanDriverName']).read_bytes()).hexdigest()==driver['manifestSha256']
env=dict(os.environ,VK_DRIVER_FILES=driver['VulkanDriverName'],VK_ICD_FILENAMES=driver['VulkanDriverName'],VK_LAYER_VALIDATE_SYNC='1')
run('intel-preview', ['python','tools/test_render_preview_host.py','--executable',str(executable),'--output',str(work/'intel-preview')],env)
run('intel-devtools', ['python','tools/test_devtools_host.py','--executable',str(executable),'--shader-config',str(shader),'--output',str(work/'intel-devtools')],env)
run('devtools-framing', ['python','tools/test_devtools_host.py','--executable',str(executable),'--shader-config',str(shader),'--output',str(work/'devtools-framing')])
run('inspector-package', ['python',str(work/'run_inspector_package.py')])
run('workspace', ['python','tools/test_editor_workspace.py','--executable',str(executable),'--output',str(work/'workspace')])
install=root/'build/ninja-msvc-release/release-gate/install-moved'
installed_shader=json.loads(shader.read_text(encoding='utf-8'))
installed_shader['binaryRoot']=str(install/'share/AzureRender/shaders')
descriptor=work/'installed-reload.json'
descriptor.write_text(json.dumps(installed_shader),encoding='utf-8')
run('installed-devtools',['python','tools/test_devtools_host.py','--executable',str(install/'bin/AzureRender.exe'),'--shader-config',str(descriptor),'--output',str(work/'installed-devtools')])
run('module-boundaries',['python','tools/check_module_boundaries.py','--source','.','--build-dir','build/ninja-msvc-release','--output',str(work/'module-boundaries.json')])
run('debug-provenance',['python','tools/build_provenance.py','verify','--build-dir','build/ninja-msvc-debug','--config','Debug'])
run('release-provenance',['python','tools/build_provenance.py','verify','--build-dir','build/ninja-msvc-release','--config','Release'])
