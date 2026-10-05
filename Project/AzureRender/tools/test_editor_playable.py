"""Author an exploration variant from an empty level with production editor commands."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from create_editor_tutorial import prepare_empty

SOURCE = Path(__file__).resolve().parents[1] / 'assets_public' / 'exploration'


def run(executable, root, install=None):
    game = root / 'game'
    prepare_empty(game)
    assets = game / 'assets'
    original = json.loads((SOURCE / 'assets/exploration.azurelevel').read_text(encoding='utf-8'))
    nodes = list(original['nodes'])
    resources = {resource['id']: resource['asset'] for resource in original['resources']}
    for instance in original['prefabs']:
        prefab = json.loads((SOURCE / 'assets' / instance['asset'].split(':/', 1)[1]).read_text(encoding='utf-8'))
        for resource in prefab['resources']:
            resources[instance['instance'] + ':' + resource['id']] = resource['asset']
        for item in prefab['nodes']:
            local = item['id']
            # G6 templates override only transform translation.
            override = instance.get('overrides', {}).get(local, {})
            for component, envelope in override.get('components', {}).items():
                item.setdefault('components', {}).setdefault(component, {}).setdefault('data', {}).update(envelope.get('data', {}))
            item['id'] = instance['instance'] + ':' + local
            item['resourceId'] = instance['instance'] + ':' + item['resourceId']
            nodes.append(item)
    actions = []
    frame = 1

    def action(command, **values):
        actions.append({'frame': frame, 'command': command, **values})

    for reference in sorted(set(resources.values())):
        action('import', path=str((SOURCE / 'assets' / reference.split(':/', 1)[1]).resolve()), key=reference)
        frame += 1
    for node in nodes:
        if node.get('resourceId'):
            action('place', id=node['id'], resource=resources[node['resourceId']])
        else:
            action('node', id=node['id'])
    frame += 1
    for node in nodes:
        action('select', id=node['id'])
        for component, envelope in node.get('components', {}).items():
            if component not in ('azure.transform', 'azure.renderable'):
                action('component-add', type=component)
            for field, value in envelope.get('data', {}).items():
                action('component-field', type=component, field=field, value=value)
        frame += 1
    action('select', id='hero:body')
    action('debug-overlay', enabled=True)
    action('component-field', type='azure.script', field='asset', value='assets:/absent.lua', expectError=True)
    action('preview', state='walk', time=0.1, previous='idle', crossfade=0.18)
    frame += 1
    action('clear-preview')
    action('prefab', asset='assets:/prefabs/collectible.azureprefab', instance='author-probe')
    action('undo')
    action('redo')
    action('delete')
    action('save')
    action('reload')
    frame += 1
    action('select', id='hero:body')
    action('transform', translation=[0, 0, 1])
    action('undo')
    action('play')
    frame += 4
    action('write-script', path='assets:/hero.lua', value="function update(dt) error('authoring fault') end")
    frame += 3
    action('write-script', path='assets:/hero.lua', value=(SOURCE / 'assets/hero.lua').read_text(encoding='utf-8'))
    frame += 3
    action('pause')
    action('step')
    frame += 2
    action('resume')
    frame += 3
    action('stop')
    action('save')
    if install:
        action('build', install=str(install.resolve()), output=str((root / 'package').resolve()))
        frame += 1
        action('wait-build')
    task = root / 'actions.json'
    task.write_text(json.dumps(actions, indent=2), encoding='utf-8')
    report = root / 'editor-runtime.json'
    startup = None
    if os.name == 'nt':
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
    process = subprocess.run([str(executable.resolve()), '--editor-project', str((game / 'project.azureproject').resolve()),
        '--editor-actions', str(task.resolve()), '--runtime-report', str(report.resolve()), '--fixed-frame-step',
        '--smoke-frames', str(frame + 8)], cwd=root,
        capture_output=True, encoding='utf-8', errors='replace', timeout=240, startupinfo=startup)
    (root / 'stdout.log').write_text(process.stdout, encoding='utf-8')
    (root / 'stderr.log').write_text(process.stderr, encoding='utf-8')
    assert process.returncode == 0, process.stdout + process.stderr
    assert 'VUID-' not in process.stderr and 'Validation Error' not in process.stderr, process.stderr
    assert 'Allocator after unload: buffers=0 images=0' in process.stdout
    data = json.loads(report.read_text(encoding='utf-8'))
    assert len(data['editorActions']) == len(actions) and all(x['passed'] for x in data['editorActions']), data
    assert data['editStateRestored'] and not data['editorPlaying'], data
    assert data['observedScriptErrors'] >= 1 and data['recoveredScripts'] >= 7, data
    assert not data['presentationErrors'], data
    assert data['editorPreviewFrames'] >= 1 and data['editorDebugLines'] > 0, data
    saved = json.loads((assets / 'exploration.azurelevel').read_text(encoding='utf-8'))
    assert len(saved['nodes']) == len(nodes) and len(saved['resources']) == 3, saved
    hero = next(node for node in saved['nodes'] if node['id'] == 'hero:body')
    assert hero['components']['azure.transform']['data']['translation'] == [0, 0, 0], hero
    assert hero['components']['azure.animator']['data']['locomotion'], hero
    assert hero['components']['azure.task-state']['data']['collected'] == 0, hero
    assert not saved.get('prefabs'), saved
    if install:
        assert data['gameBuildPassed'], data
    summary = {'actions': len(actions), 'nodes': len(nodes), 'resources': len(saved['resources']),
               'editStateRestored': True, 'scriptRecovery': True, 'validationErrors': 0, 'built': bool(install)}
    (root / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(summary))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', required=True, type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--install', type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='azure_authoring_') as temporary:
        run(args.executable, args.output.resolve() if args.output else Path(temporary), args.install)
