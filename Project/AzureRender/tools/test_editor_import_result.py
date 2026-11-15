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


def wait_for_task(host, identity):
    deadline = time.monotonic() + 30
    while time.monotonic() < deadline:
        host.frames()
        task = next(task for task in host.workspace()['tasks'] if task['id'] == identity)
        if task['state'] != 'running':
            return task
    raise AssertionError('Import did not complete: ' + identity)


def reveal_from_scrolled_browse(host, grid):
    rows = {row['path']: row['id'] for row in host.edit('assets.catalog', type='text')}
    initial = 'asset.' + rows['assets:/browse/entry-00.txt']
    target = rows['assets:/reveal/entry-59.txt']
    host.click('directory.assets:/browse')
    host.click('assets.type')
    host.click('type.Text')
    host.click('assets.search')
    host.call('input', event={'action': 'text', 'text': 'entry'})
    host.frames()
    if grid:
        host.click('assets.grid')
    host.click(initial)
    host.call('input', event={'action': 'wheel', 'y': -4})
    host.frames(20)
    before = host.workspace()
    assert before['contentView']['grid'] == grid
    assert before['contentView']['scroll'] > 0
    assert before['contentNavigation']['selected'] == [initial.removeprefix('asset.')]
    assert 'asset.' + target not in before['widgets']
    host.edit('reference.reveal', kind='asset', id=target)
    host.frames(12)
    revealed = host.workspace()
    assert revealed['contentNavigation']['selected'] == [target]
    assert revealed['contentNavigation']['path'] == 'reveal'
    assert revealed['contentView']['scroll'] > 0
    _, y, _, height = revealed['widgets']['asset.' + target]
    _, panel_y, _, panel_height = revealed['widgets']['assets.drop-region']
    assert panel_y < y + height / 2 < panel_y + panel_height
    host.click('assets.return-to-browse')
    restored = host.workspace()
    for key in ('mount', 'path', 'selected'):
        assert restored['contentNavigation'][key] == before['contentNavigation'][key]
    for key in ('search', 'type', 'scroll'):
        assert restored['contentView'][key] == before['contentView'][key]


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
    for directory in ('browse', 'reveal'):
        folder = project / 'assets' / directory
        folder.mkdir()
        for index in range(60):
            (folder / f'entry-{index:02d}.txt').write_text('Asset Browser fixture', encoding='utf-8')
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
        node = host.query('selection.id')
        slot = host.edit('material.slots', node=node)['slots'][0]['id']
        material = host.edit('material.create', type='azure.pbr',
                             destination='assets:/Review.azurematerial')['asset']
        host.click('material.slots-header')
        host.click('material.slot.' + slot)
        host.click('material.assign.' + slot + '.' + material)
        assert host.edit('material.slots', node=node)['slots'][0]['override'] == material
        assert 'material.open.' + slot in host.workspace()['widgets'], 'An assigned material must open from its inspector slot'
        host.click('material.open.' + slot)
        initial_material = host.edit('material.describe', asset=material)['document']
        host.click('material.field.roughness')
        changed_material = host.edit('material.describe', asset=material)['document']
        assert initial_material != changed_material, 'The material control must edit the assigned asset'
        host.click('material.undo')
        assert host.edit('material.describe', asset=material)['document'] == initial_material
        host.click('material.redo')
        assert host.edit('material.describe', asset=material)['document'] == changed_material
        host.click('material.close')
        assert host.query('selection.id') == node
        host.click('save')
    with EditorHost(executable, project_file, root / 'reopened') as host:
        assert host.workspace()['nodeCount'] == initial_count + 1
        assert host.edit('material.slots', node=node)['slots'][0]['override'] == material
        assert host.edit('material.describe', asset=material)['document'] == changed_material
        reveal_from_scrolled_browse(host, grid=fixture == 'exploration')
        dropped = host.edit('asset.import-drop', path=str(source), place=False)
        drop_task = wait_for_task(host, dropped)
        assert drop_task['state'] == 'completed', drop_task
        host.click('task.asset-reveal.' + dropped)
        assert host.workspace()['contentNavigation']['selected'] == [drop_task['result']['asset']]
        broken = source.with_name('missing-dependency.gltf')
        document = json.loads(source.read_text(encoding='utf-8'))
        document['buffers'][0]['uri'] = 'missing-fixture.bin'
        broken.write_text(json.dumps(document), encoding='utf-8')
        failed = host.edit('asset.import-drop', path=str(broken), place=True)
        failure = wait_for_task(host, failed)
        assert failure['state'] == 'failed' and 'missing-fixture.bin' in failure['diagnostic'], failure
        assert host.workspace()['nodeCount'] == initial_count + 1
        host.click('task.import-error.' + failed)
        assert 'task.import-error-detail.' + failed in host.workspace()['widgets']
        host.call('input', event={'action': 'key', 'key': 'Escape', 'down': True})
        host.frames()
        host.call('input', event={'action': 'key', 'key': 'Escape', 'down': False})
        host.frames()
        if fixture == 'scene_inspector':
            host.click('save')
            second = root / 'other-project'
            create(second)
            opened = host.edit('project.open', path=str(second / 'project.azureproject'))
            host.frames(12)
            switched = host.workspace()
            assert switched['project']['id'] == opened['project']
            assert switched['contentNavigation']['selected'] == []
            assert 'task.import-error.' + failed not in switched['widgets']
            assert 'assets.return-to-browse' not in switched['widgets']
    return {'project': fixture, 'asset': asset, 'placedAndReopened': True,
            'browseRestored': True, 'assignedMaterialEditedAndReopened': True,
            'offscreenReveal': 'grid' if fixture == 'exploration' else 'list',
            'dropReceipt': True, 'missingDependencyRecovery': True}


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
