"""Repair and verify the documentation-only installation rule."""
import hashlib
import json
from pathlib import Path
import subprocess
import tarfile

root = Path(__file__).resolve().parents[3]
work = root / 'build/evolution/r7'

def run(name, args):
    with (work / (name + '.log')).open('w', encoding='utf-8') as log:
        subprocess.run(args, cwd=root, stdout=log, stderr=subprocess.STDOUT, check=True)
    print(name + ' passed', flush=True)

for configuration in ('Debug', 'Release'):
    build = root / ('build/ninja-msvc-' + configuration.lower())
    old = json.loads((build / 'build-provenance.json').read_text())
    changed = [name for name, expected in old['source'].items()
               if hashlib.sha256((root / name).read_bytes()).hexdigest() != expected]
    assert changed == ['CMakeLists.txt'], changed
    (work / ('pre-document-repair-' + configuration.lower() + '-provenance.json')).write_text(json.dumps(old, indent=2), encoding='utf-8')
    run('document-repair-' + configuration.lower() + '-build', ['python', 'tools/build_provenance.py', 'build', '--build-dir', str(build), '--config', configuration])
    updated = json.loads((build / 'build-provenance.json').read_text())
    assert old['products'] == updated['products'], 'Documentation installation changed a runtime product'
run('document-repair-build-configuration', ['ctest', '--test-dir', 'build/ninja-msvc-release', '-C', 'Release', '-R', '^AzureEngine.BuildConfiguration$', '--output-on-failure'])
package = work / 'packages/final-AzureRender-0.1.0-rc1-Windows-x64.tar.gz'
if package.exists():
    package.rename(package.with_name('missing-preview-doc-' + package.name))
run('package-finalize', ['python', str(work / 'finalize_package.py')])
run('evidence-collect', ['python', str(work / 'collect_evidence.py')])
install = root / 'build/ninja-msvc-release/release-gate/install-moved/share/AzureRender'
for name in ('docs/acceptance/r7/2026-10-06.md', 'docs/runtime/development-previews.md'):
    assert (install / name).read_bytes() == (root / name).read_bytes(), name
result = json.loads((work / 'final-package-result.json').read_text())
with tarfile.open(result['package']) as archive:
    entry = next(row for row in archive.getmembers() if row.name.endswith('/docs/runtime/development-previews.md'))
    assert archive.extractfile(entry).read() == (root / 'docs/runtime/development-previews.md').read_bytes()
assert hashlib.sha256(Path(result['package']).read_bytes()).hexdigest() == result['sha256']
run('docs-check-final', ['D:/Git/Git/bin/bash.exe', 'tools/check_docs.sh'])
run('site-final', ['python', '-m', 'mkdocs', 'build', '--strict', '--site-dir', str(work / 'site-final')])
run('evidence-collect', ['python', str(work / 'collect_evidence.py')])
manifest = json.loads((root / 'docs/acceptance/r7/evidence/2026-10-06/manifest.json').read_text())
for name, expected in manifest['files'].items():
    assert hashlib.sha256((root / 'docs/acceptance/r7/evidence/2026-10-06' / name).read_bytes()).hexdigest() == expected, name
for name, expected in manifest['implementationInputs'].items():
    assert hashlib.sha256((root.parent.parent / name).read_bytes()).hexdigest() == expected, name
print('R7 installation, package documentation and evidence hashes verified', flush=True)
