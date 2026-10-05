"""Publish and move a statically extended Player without editor linkage."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools'))
import build_game
from run_playable_long_run import isolated_environment

work = root / 'build/evolution/f4'
fixture = work / 'registered-player-release'
player = fixture / 'AzureRegisteredPlayer.exe'
commands = subprocess.check_output(['ninja', '-C', str(fixture), '-t', 'commands', 'AzureRegisteredPlayer'], text=True)
links = [line for line in commands.splitlines() if '/out:AzureRegisteredPlayer.exe' in line]
assert len(links) == 1, 'Registered Player link command missing'
assert 'AzureEditor.lib' not in links[0] and 'azure_imgui.lib' not in links[0], links[0]
(work / 'registered-player-link.txt').write_text(links[0] + '\n')
startup = subprocess.STARTUPINFO()
startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
startup.wShowWindow = 0
with tempfile.TemporaryDirectory(prefix='registered-package-', dir=work) as temporary:
    scratch = Path(temporary)
    project = scratch / 'Tool project'
    shutil.copytree(root / 'assets_public/gameplay', project)
    level_file = project / 'assets/courtyard.azurelevel'
    level = json.loads(level_file.read_text())
    hero = next(node for node in level['nodes'] if node['id'] == 'hero')
    hero['components']['tool.asset-note'] = {'type': 'tool.asset-note', 'version': 1,
                                          'data': {'text': 'portable inspection', 'sampleCount': 3}}
    level_file.write_text(json.dumps(level, indent=2) + '\n')
    rejected = subprocess.run([str(root / 'build/ninja-msvc-release/AzurePlayer.exe'),
        '--project', str(project / 'project.azureproject'), '--check-project'],
        capture_output=True, text=True, encoding='utf-8', errors='replace', startupinfo=startup, timeout=60)
    assert rejected.returncode != 0 and 'tool.asset-note' in rejected.stdout + rejected.stderr
    install = scratch / 'Registered engine'
    shutil.copytree(root / 'build/ninja-msvc-release/release-gate/install-moved', install)
    shutil.copy2(player, install / 'bin/AzurePlayer.exe')
    published = scratch / 'Published inspection'
    manifest = build_game.build_game(project / 'project.azureproject', install, published)
    moved = scratch / 'Moved registered project with spaces'
    published.rename(moved)
    project.rename(scratch / 'Project unavailable')
    install.rename(scratch / 'Engine unavailable')
    env = isolated_environment()
    command = [env['COMSPEC'], '/d', '/c', 'start-game.cmd', '--fixed-frame-step', '--smoke-frames', '18',
               '--runtime-report', str(work / 'registered-package-runtime.json')]
    result = subprocess.run(command, cwd=moved, env=env, capture_output=True,
        text=True, encoding='utf-8', errors='replace', startupinfo=startup, timeout=90)
    (work / 'registered-package.log').write_text(result.stdout + result.stderr, encoding='utf-8')
    assert result.returncode == 0, result.stdout + result.stderr
    assert 'Allocator after unload: buffers=0 images=0' in result.stdout
    assert 'VUID-' not in result.stderr and 'Validation Error' not in result.stderr
    data = json.loads((work / 'registered-package-runtime.json').read_text())
    assert data['fixedSteps'] > 0 and not data['scriptErrors'] and not data['presentationErrors'], data
    build_game.verify_package(moved)
    package_manifest = hashlib.sha256((moved / 'game_manifest.json').read_bytes()).hexdigest()
report = {'schemaVersion': 1, 'status': 'passed', 'ordinaryPlayerRejectedUnknownType': True,
    'registeredPlayerLoaded': True, 'editorLinked': False, 'movedDirectory': True,
    'isolatedPath': env['PATH'], 'sourceUnavailable': True, 'engineUnavailable': True,
    'files': len(manifest['files']), 'packageManifestHash': package_manifest,
    'playerSha256': hashlib.sha256(player.read_bytes()).hexdigest(), 'released': True}
(work / 'registered-package.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report), flush=True)
