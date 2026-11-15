"""Exercise viewport observation modes without writing the scene."""
import argparse
import json
from pathlib import Path
import shutil
import uuid

from create_playable_project import create
from editor_test_host import EditorHost


def press(host, key):
    for down in (True, False):
        host.call('input', event=dict(action='key', key=key, down=down))
        host.frames()


def diagnostic_view(host):
    return int(host.query('render.diagnosticView'))


def capture_auxiliary_view(host, label):
    handle = host.edit('preview.create', extent=[160, 120],
                       position=[2.8, 2.1, 3.2], target=[0, 0, 0])['handle']
    host.frames()
    output = host.edit('preview.capture', handle=handle, label=label)
    host.edit('preview.release', handle=handle)
    return Path(output['path']).read_bytes()


def run(executable, root):
    assets = Path(__file__).resolve().parents[1] / 'assets_public'
    project = root / 'inspector'
    shutil.copytree(assets / 'scene_inspector', project, ignore=shutil.ignore_patterns('.azure'))
    project_file = project / 'project.azureproject'
    level = project / 'assets/inspection.azurelevel'
    original = level.read_bytes()
    with EditorHost(executable, project_file, root / 'edit') as host:
        state = host.workspace()
        original_hash = host.query('document.contentHash')
        assert diagnostic_view(host) == 0
        host.screenshot('editor-view-beauty')
        auxiliary = capture_auxiliary_view(host, 'editor-auxiliary-before')
        host.click('viewport.diagnostics')
        host.click('viewport.diagnostic.3')
        assert diagnostic_view(host) == 3
        host.screenshot('editor-view-shadow')
        assert capture_auxiliary_view(host, 'editor-auxiliary-during') == auxiliary
        host.click('viewport.diagnostics')
        host.click('viewport.diagnostic.default')
        assert diagnostic_view(host) == 0
        host.screenshot('editor-view-restored')
        host.click('viewport.image')
        press(host, '0')
        assert diagnostic_view(host) == 1
        current = host.workspace()
        assert host.query('document.contentHash') == original_hash
        assert current['undoCount'] == state['undoCount']
        assert current['documents'][0]['redoCount'] == state['documents'][0]['redoCount']
        assert not current['dirty']
        host.click('play')
        assert host.workspace()['previewMode'] == 'play'
        assert diagnostic_view(host) == 0, 'The game view must use the scene display'
        host.click('stop')
        stopped = host.workspace()
        assert stopped['previewMode'] == 'edit' and diagnostic_view(host) == 1
        assert stopped['camera'] == current['camera']
        assert stopped['selection'] == current['selection']
        assert host.query('document.contentHash') == original_hash
        host.click('save')
        # Some scene encoders add default fields on explicit save.
        before = json.loads(original).get('renderSettings', {})
        after = json.loads(level.read_bytes()).get('renderSettings', {})
        assert after.get('diagnosticView', 0) == before.get('diagnosticView', 0)
    with EditorHost(executable, project_file, root / 'reopen') as host:
        assert diagnostic_view(host) == 0
        assert not host.workspace()['dirty']
    # Empty exploration-derived scene uses a renderer with only a final display.
    second = root / 'exploration'
    create(second)
    descriptor = json.loads((second / 'project.azureproject').read_text(encoding='utf-8'))
    level = second / 'assets' / descriptor['startupScene'].split(':/', 1)[1]
    document = json.loads(level.read_text(encoding='utf-8'))
    for section in ('nodes', 'resources', 'lights', 'prefabs'):
        document[section] = []
    level.write_text(json.dumps(document) + '\n', encoding='utf-8')
    with EditorHost(executable, second / 'project.azureproject', root / 'limited') as host:
        assert 'viewport.diagnostics' not in host.workspace()['widgets']
        initial = diagnostic_view(host)
        host.click('viewport.image')
        press(host, '0')
        assert diagnostic_view(host) == initial
        assert not host.workspace()['dirty']
    return {'modeSwitch': True, 'keyRoute': True, 'restored': True, 'savedSceneUnchanged': True,
            'playUsesSceneMode': True, 'stopRestoresSelectionAndCamera': True,
            'auxiliaryPreservesSceneMode': True}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = run(args.executable.resolve(), args.output.resolve() / ('run-' + uuid.uuid4().hex[:8]))
    print(json.dumps(result))
