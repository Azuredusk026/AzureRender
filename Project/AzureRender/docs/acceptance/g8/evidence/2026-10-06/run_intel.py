"""Verify this stage's moved package through the registered Intel driver."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools'))
from run_playable_long_run import isolated_environment
from test_playable_player import run

out = root / 'build/evolution/g8/intel'
out.mkdir(parents=True, exist_ok=True)
drivers = json.loads((root / 'docs/acceptance/p1/evidence/2026-10-05/driver-inventory.json').read_text())
driver = next(row for row in drivers if row['DriverDesc'] == 'Intel(R) UHD Graphics')
assert hashlib.sha256(Path(driver['VulkanDriverName']).read_bytes()).hexdigest() == driver['manifestSha256']
env = isolated_environment()
env.update(VK_DRIVER_FILES=driver['VulkanDriverName'], VK_ICD_FILENAMES=driver['VulkanDriverName'])
package = root / 'build/ninja-msvc-release/playable-package/Azure Exploration'
startup = subprocess.STARTUPINFO()
startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
startup.wShowWindow = 0
command = [str(package / 'bin/AzurePlayer.exe'), '--project', str(package / 'game/project.azureproject'),
    '--width', '960', '--height', '540', '--fixed-frame-step', '--smoke-frames', '30',
    '--gpu-timing', '--gpu-timing-output', str(out / 'probe.json')]
probe = subprocess.run(command, cwd=out, env=env, capture_output=True,
    encoding='utf-8', errors='replace', timeout=120, startupinfo=startup)
(out / 'stdout.log').write_text(probe.stdout, encoding='utf-8')
(out / 'stderr.log').write_text(probe.stderr, encoding='utf-8')
assert probe.returncode == 0, probe.stderr
cap = json.loads((out / 'captures/gpu_capabilities.json').read_text())
assert 'Intel' in cap['device_name'], cap
run(package / 'bin/AzurePlayer.exe', out / 'quest', project=package / 'game/project.azureproject', env=env, cwd=out)
result = {'status': 'passed', 'device': cap['device_name'], 'driver': driver, 'capabilities': cap,
    'configuration': '960x540, full materials, four cascades',
    'quest': json.loads((out / 'quest/summary.json').read_text())}
(out / 'summary.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result), flush=True)
