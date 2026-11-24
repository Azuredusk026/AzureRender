"""Exercise the production physics toolbar, camera capture and scene restoration."""
import argparse
import json
import shutil
import uuid
from pathlib import Path
from test_editor_workspace import launch as launch_workspace


def launch(executable, root, name, size, dpi, actions=(), **options):
    framing = [dict(frame=18,action='click',target='node.moving'),dict(frame=20,action='click',target='focus')]
    return launch_workspace(executable,root,name,size,dpi,framing+list(actions),**options)


def run(executable, output):
    output = output / ('run-' + uuid.uuid4().hex[:8])
    results = {}
    for mechanism in ('arm', 'door'):
        root = output / mechanism
        root.mkdir(parents=True)
        source = Path(__file__).resolve().parents[1] / ('assets_public/physics_' + mechanism)
        shutil.copytree(source, root / 'game', ignore=shutil.ignore_patterns('.azure'))
        start = [dict(frame=25, action='click', target='simulate')]
        pause = start + [dict(frame=30, action='click', target='pause')]
        initial = launch(executable, root, 'edit', (1920, 1080), 1, capture=False)
        running = launch(executable, root, 'running', (1920, 1080), 1, start, capture=False)
        paused = launch(executable, root, 'paused', (1920, 1080), 1, pause, capture=False)
        stepped = launch(executable, root, 'step', (1920, 1080), 1, pause + [dict(frame=34, action='click', target='step')], capture=False)
        assert initial['previewMode'] == 'edit'
        assert running['previewMode'] == 'simulate' and running['previewSteps'] > 0
        assert running['physicsConstraints'] == 1, running
        assert running['camera'] == initial['camera'], 'Simulation must retain the editor camera'
        assert stepped['previewSteps'] == paused['previewSteps'] + 1
        assert any(body['node'] == 'moving' for body in running['physicsBodies'])
        if mechanism == 'arm':
            assert next(b for b in running['physicsBodies'] if b['node'] == 'moving')['position'][1] < 3
        navigation = launch(executable, root, 'navigation', (1920, 1080), 1, pause + [
            dict(frame=34, action='mouse', target='viewport.image', button='right', down=True),
            dict(frame=36, action='key', key='W', down=True), dict(frame=42, action='key', key='W', down=False),
            dict(frame=44, action='mouse', target='viewport.image', button='right', down=False)], capture=False)
        assert navigation['camera'] != paused['camera'], 'Held right mouse navigation works during simulation'
        assert navigation['previewSteps'] == paused['previewSteps']
        assert not navigation['navigation']['cursorCaptured'] and navigation['navigation']['nativeCursorMode'] == 'normal'
        stopped = launch(executable, root, 'stop', (1920, 1080), 1, pause + [dict(frame=34, action='click', target='stop')], capture=False)
        assert stopped['previewMode'] == 'edit' and not stopped['dirty'] and stopped['undoCount'] == initial['undoCount']
        axis = launch(executable, root, 'axis', (1920, 1080), 1, pause + [dict(frame=34, action='click', target='viewport.axis.+Y')], capture=True)
        assert axis['view']['projection'] == 'orthographic' and axis['previewMode'] == 'simulate'
        choose = [dict(frame=25, action='click', target='node.moving'),dict(frame=28, action='click', target='physics.shape'),dict(frame=33, action='click', target='physics.shape.sphere')]
        authored = launch(executable, root, 'authoring', (1920,1080),1,choose,capture=False,close_policy='discard')
        assert authored['dirty'] and authored['undoCount']==1,'Shape chooser uses the production document transaction'
        undone = launch(executable, root, 'authoring-undo', (1920,1080),1,choose+[
            dict(frame=38,action='key',key='Z',ctrl=True),dict(frame=39,action='key',key='Z',down=False,ctrl=True)],capture=False,close_policy='discard')
        assert not undone['dirty'] and undone['undoCount']==0,'Shape edit can be undone'
        for name in ('running', 'paused', 'step', 'navigation', 'stop', 'axis'):
            report = json.loads((root / name / 'runtime.json').read_text(encoding='utf-8'))
            baseline = json.loads((root / 'edit/runtime.json').read_text(encoding='utf-8'))
            assert report['editorWorkspace']['document']['contentHash'] == baseline['editorWorkspace']['document']['contentHash'], name
        results[mechanism] = dict(constraints=running['physicsConstraints'], pausedSteps=paused['previewSteps'], singleStep=stepped['previewSteps'], cameraNavigation=True, restored=True)
    (output / 'summary.json').write_text(json.dumps(results, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(results, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    run(args.executable.resolve(), args.output.resolve())
