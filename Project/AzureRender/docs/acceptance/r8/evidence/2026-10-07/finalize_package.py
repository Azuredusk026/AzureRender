import hashlib
import json
from pathlib import Path
import subprocess
import shutil

root = Path(__file__).resolve().parents[3]
build = root / 'build/ninja-msvc-release'
gate = build / 'release-gate'
install = gate / 'install-moved'
logs = root / 'build/evolution/r8'
def run(name, args):
    result = subprocess.run(args, cwd=root, capture_output=True, text=True)
    (logs / (name + '.log')).write_text(result.stdout + result.stderr, encoding='utf-8')
    if result.returncode:
        raise RuntimeError(name + ': ' + result.stderr[-2000:])
gate_result = json.loads((gate / 'result.json').read_text())
original = Path(gate_result['package'])
archives = logs / 'packages'
archives.mkdir(parents=True, exist_ok=True)
raw_package = archives / ('gate-' + original.name)
if not raw_package.exists():
    assert hashlib.sha256(original.read_bytes()).hexdigest() == gate_result['sha256'], 'Raw gate package differs'
    shutil.copy2(original, raw_package)
assert hashlib.sha256(raw_package.read_bytes()).hexdigest() == gate_result['sha256'], 'Saved gate package differs'
run('final-document-configure', ['cmake', '-S', str(root), '-B', str(build)])
run('final-install', ['cmake', '--install', str(build), '--config', 'Release', '--prefix', str(install)])
manifest = install / 'install_manifest.json'
for name, script in [('final-install-manifest', 'write_install_manifest'), ('final-install-verify', 'verify_install_manifest')]:
    key = 'OUTPUT_FILE' if name.endswith('manifest') else 'MANIFEST_FILE'
    run(name, ['cmake', '-DINSTALL_DIR=' + str(install), '-' + 'D' + key + '=' + str(manifest), '-P', 'tools/' + script + '.cmake'])
run('final-package', ['cpack', '-G', 'TGZ', '-C', 'Release', '-B', str(build), '--config', str(build / 'CPackConfig.cmake')])
result = json.loads((gate / 'result.json').read_text())
package = Path(result['package'])
run('final-package-manifest', ['cmake', '-DPACKAGE_FILE=' + str(package), '-DOUTPUT_FILE=' + str(package) + '.manifest.json', '-P', 'tools/write_rc_manifest.cmake'])
result['sha256'] = hashlib.sha256(package.read_bytes()).hexdigest()
result['size_bytes'] = package.stat().st_size
final_package = archives / ('final-' + package.name)
shutil.copy2(package, final_package)
result['package'] = str(final_package)
result['originalGatePackage'] = {'package': str(raw_package), 'sha256': gate_result['sha256']}
result['documentationRefresh'] = 'final phase documentation installed and packaged after source and GPU gates'
(logs / 'final-package-result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
print(json.dumps(result))

