"""Exercise scene-container drag/drop and the authoring workspace via ImGui."""
import argparse
import json
import shutil
import uuid
from pathlib import Path
from editor_test_host import EditorHost


def run(executable, output):
    root = output / ('run-' + uuid.uuid4().hex[:8])
    source = Path(__file__).resolve().parents[1] / 'assets_public/scene_inspector'
    shutil.copytree(source, root / 'project', ignore=shutil.ignore_patterns('.azure'))
    project = root / 'project/project.azureproject'
    with EditorHost(executable, project, root / 'editor') as host:
        state = host.workspace()
        panels = state['panels']
        assert panels['outliner']['rect'][0] < panels['viewport']['rect'][0] < panels['inspector']['rect'][0]
        assert not panels['capture']['open'] and not panels['build']['open']
        assert 'outliner.add' not in state['widgets']
        assert 'hierarchy.root' in state['widgets']
        host.click('hierarchy.root')
        assert host.workspace()['selection']==[]
        host.click('hierarchy.empty')
        host.click('outliner.create'); host.click('preset.empty')
        parent = host.workspace()['selection'][0]
        host.click('outliner.create'); host.click('preset.empty')
        child = host.workspace()['selection'][0]
        before = host.workspace()['undoCount']
        host.call('input', event=dict(action='mouse', target='node.' + child, down=True)); host.frames()
        host.call('input', event=dict(action='mouse', target='node.' + child, offset=[12, 0], down=True)); host.frames()
        host.call('input', event=dict(action='mouse', target='hierarchy.empty', down=True)); host.frames()
        host.call('input', event=dict(action='mouse', target='hierarchy.empty', down=False)); host.frames()
        assert host.workspace()['undoCount'] == before + 1
        host.edit('document.save')
        config = json.loads(project.read_text(encoding='utf-8'))
        level_path = root / 'project/assets' / config['startupScene'].split(':/', 1)[1]
        level = json.loads(level_path.read_text(encoding='utf-8'))
        assert next(n for n in level['nodes'] if n['id'] == child)['parentId'] == ''
        host.edit('history.undo'); host.edit('document.save')
        level = json.loads(level_path.read_text(encoding='utf-8'))
        assert next(n for n in level['nodes'] if n['id'] == child)['parentId'] == parent
        host.click('mode.Rotate')
        assert host.workspace()['gizmoMode'] == 1
        scripts=host.edit('assets.catalog',type='script')
        script=next(row for row in scripts if row['path'].endswith('.lua'))
        host.click('script.attach-picker');host.click('script.attach.'+script['id'])
        host.edit('document.save')
        level=json.loads(level_path.read_text(encoding='utf-8'))
        component=next(n for n in level['nodes'] if n['id']==child)['components']['azure.script']
        assert component['data']['asset']==script['id']
        host.edit('preview.simulate');host.frames()
        before=host.workspace()
        host.click('viewport.image',button='right')
        host.click('viewport.duplicate')
        assert host.workspace()['nodeCount']==before['nodeCount']
        assert host.workspace()['feedback']==before['feedback']
        host.call('input',event=dict(action='key',key='Escape',down=True));host.frames()
        host.call('input',event=dict(action='key',key='Escape',down=False));host.frames()
        host.edit('preview.stop');host.frames()
        host.screenshot('authoring-workspace')
        host.click('menu.Edit');host.click('menu.project-settings')
        settings=host.workspace()['widgets']
        assert 'setting.render.exposure' in settings
        assert not any(key.startswith('setting.render.features.') or key=='setting.render.blackholeQuality' for key in settings)
        assert 'settings.source' not in settings
        host.screenshot('project-settings')
    (root / 'summary.json').write_text(json.dumps(dict(passed=True, physicalInput=False,
        sceneContainer=True, emptySpaceUnparent=True, undoRestoresParent=True,
        layout=True, viewportTools=True), indent=2), encoding='utf-8')
    print(root)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    run(args.executable.resolve(), args.output.resolve())
