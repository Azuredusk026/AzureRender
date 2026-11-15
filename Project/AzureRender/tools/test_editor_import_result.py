"""Locate and place a model from a production import receipt."""
import argparse
import json
from pathlib import Path
import shutil
import time
import uuid

from create_playable_project import create
from editor_test_host import EditorHost


def wait_for_import(host):
    deadline = time.monotonic() + 30
    while time.monotonic() < deadline:
        host.frames()
        workspace = host.workspace()
        task = next((task for task in workspace['tasks']
                     if task['source'].startswith('asset.import: ')), None)
        if task and task['state'] != 'running':
            assert task['state'] == 'completed', task
            return task, workspace
    raise AssertionError('Import did not complete')


def run(executable, root, fixture):
    assets = Path(__file__).resolve().parents[1] / 'assets_public'
    project = root / 'game'
    if fixture == 'exploration':
        create(project)
    else:
        shutil.copytree(assets / 'scene_inspector', project, ignore=shutil.ignore_patterns('.azure'))
    source = root / '新模型 输入' / 'review-cube.gltf'
    source.parent.mkdir()
    shutil.copy2(assets / 'scene_inspector/assets/cube.gltf', source)
    project_file = project / 'project.azureproject'
    with EditorHost(executable, project_file, root / 'editor') as host:
        host.click('assets.search')
        host.call('input', event={'action': 'text', 'text': 'original filter'})
        host.frames()
        host.click('assets.import-create')
        host.click('import.path')
        host.call('input', event={'action': 'text', 'text': str(source)})
        host.frames()
        host.click('asset.import')
        task, workspace = wait_for_import(host)
        asset = task['result']
        locate = 'task.asset-reveal.' + task['id']
        assert locate in workspace['widgets'], 'Completed import must offer a direct locator'
        host.click(locate)
        visible = host.workspace()
        assert visible['contentNavigation']['selected'] == [asset]
        assert 'asset.' + asset in visible['widgets']
        host.click('assets.return-to-browse')
        restored = host.workspace()
        assert restored['contentView']['search'] == 'original filter'
        assert restored['contentNavigation']['selected'] == workspace['contentNavigation']['selected']
        host.click(locate)
        initial_count = host.workspace()['nodeCount']
        host.click('asset.' + asset, button='right')
        host.click('asset.place')
        assert host.workspace()['nodeCount'] == initial_count + 1
        host.click('menu.Edit')
        host.click('menu.Undo')
        assert host.workspace()['nodeCount'] == initial_count
        host.click('menu.Edit')
        host.click('menu.Redo')
        assert host.workspace()['nodeCount'] == initial_count + 1
        host.click('save')
    with EditorHost(executable, project_file, root / 'reopened') as host:
        assert host.workspace()['nodeCount'] == initial_count + 1
    return {'project': fixture, 'asset': asset, 'placedAndReopened': True, 'browseRestored': True}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    root = args.output.resolve() / ('run-' + uuid.uuid4().hex[:8])
    cases = [run(args.executable.resolve(), root / fixture, fixture)
             for fixture in ('scene_inspector', 'exploration')]
    (root / 'summary.json').write_text(json.dumps(cases, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(cases))
