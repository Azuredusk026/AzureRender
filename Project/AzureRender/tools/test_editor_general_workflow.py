"""Author a fresh scene via shared production services and verify reopening."""
import argparse
import json
from pathlib import Path
import shutil
import time
import uuid
from editor_test_host import EditorHost
from prepare_editor_review_assets import prepare


def wait_task(host, task_id):
    deadline = time.monotonic() + 60
    while time.monotonic() < deadline:
        host.frames()
        task = next(item for item in host.workspace()['tasks'] if item['id'] == task_id)
        if task['state'] not in ('queued', 'running'):
            assert task['state'] == 'completed', task
            return task['result']
    raise AssertionError('Timed out waiting for ' + task_id)


def run(executable, output):
    if output.exists():
        output = output / ('run-' + uuid.uuid4().hex[:8])
    output.mkdir(parents=True, exist_ok=True)
    source = Path(__file__).resolve().parents[1]
    shutil.copytree(source / 'assets_public/scene_inspector', output / 'bootstrap',
                    ignore=shutil.ignore_patterns('.azure'))
    prepare(output / 'review-assets')
    project_root = output / 'Monica 中文项目'
    with EditorHost(executable, output / 'bootstrap/project.azureproject', output / 'create') as host:
        task = host.edit('project.create', templateId='scene', destination=str(project_root), name='Monica')
        wait_task(host, task['task'])
        host.frames(12)
    project = project_root / 'project.azureproject'
    metadata = json.loads(project.read_text(encoding='utf-8'))
    level_path = project_root / 'assets' / metadata['startupScene'].split(':/', 1)[1]
    with EditorHost(executable, project, output / 'author') as host:
        initial = json.loads(level_path.read_text(encoding='utf-8'))['nodes']
        for node in initial:
            host.edit('node.select', id=node['id'])
            host.edit('node.delete')
        assert host.workspace()['nodeCount'] == 0
        host.edit('node.create-preset', preset='cube')
        cube = host.query('selection.id')
        host.edit('node.rename', value='Monica 基础立方体')
        host.edit('component.add', type='azure.audio-source')
        script = host.edit('script.template', backend='lua', path='assets:/脚本 复核.lua')
        assert script['asset'] and (project_root / 'assets/脚本 复核.lua.azmeta').is_file()
        material = host.edit('material.create', type='azure.pbr', destination='assets:/Monica.azurematerial')['asset']
        slots = host.edit('material.slots', node=cube)['slots']
        assert slots
        host.edit('material.assign', nodes=[cube], slotId=slots[0]['id'], material=material)
        for name in ('review-cube.gltf', 'review-checker.png', 'review-tone.wav'):
            task = host.edit('asset.import-drop', path=str(output / 'review-assets' / name),
                             place=name.endswith('.gltf'), position=[3, 0, 0])
            imported = wait_task(host, task)
            assert imported['placed'] == name.endswith('.gltf')
        host.edit('node.create-preset', preset='light')
        host.edit('node.create-preset', preset='camera')
        host.edit('document.save')
        host.frames(12)
        host.screenshot('ed7-general-workflow')
        assert not host.query('editor.dirty')
    saved = json.loads(level_path.read_text(encoding='utf-8'))
    node = next(item for item in saved['nodes'] if item['id'] == cube)
    assert node['name'] == 'Monica 基础立方体' and node['materialSlots']
    assert 'azure.audio-source' in node['components']
    with EditorHost(executable, project, output / 'reopen') as host:
        assert host.workspace()['nodeCount'] == 4
        assert not host.query('editor.dirty')
        host.edit('node.create-preset', preset='torus')
        host.edit('document.save')
    summary = dict(passed=True, freshScene=True, project=str(project), nodesAfterReopen=4,
                   modelImageAudioImported=True, materialAssigned=True, componentsPersisted=True,
                   continuedEditing=True, interface='authenticated production services', physicalInput=False)
    (output / 'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(summary))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    run(args.executable.resolve(), args.output.resolve())
