"""Verify observation, production edits and input against two real editor workflows."""
import argparse
import json
import os
from pathlib import Path
import secrets
import shutil
import subprocess
import sys
import time
import tempfile
from create_playable_project import create
from test_validation_protocol import exchange


def run(executable, output, workflow):
    folder = output / workflow
    folder.mkdir(parents=True, exist_ok=True)
    project = folder / 'Project with spaces'
    if workflow == 'exploration':
        create(project)
        node, other = 'hero:body', 'ground'
    else:
        shutil.copytree(Path(__file__).resolve().parents[1] / 'assets_public/scene_inspector', project, ignore=shutil.ignore_patterns('.azure'))
        node, other = 'asset-a', 'asset-b'
    endpoint_file = folder / 'endpoint.json'
    runtime = folder / 'runtime.json'
    token = secrets.token_hex(32)
    env = dict(os.environ, AZURE_OBSERVATION_TEST_TOKEN=token, AZURERENDER_EDITOR_CONFIG=str(folder / 'config'))
    stdout = (folder / 'stdout.log').open('w', encoding='utf-8')
    stderr = (folder / 'stderr.log').open('w', encoding='utf-8')
    process = subprocess.Popen([str(executable), '--editor-close-policy', 'save', '--editor-project', str(project / 'project.azureproject'),
        '--width', '1280', '--height', '720', '--smoke-frames', '1200', '--fixed-frame-step',
        '--resource-root', str(folder), '--validation-token-env', 'AZURE_OBSERVATION_TEST_TOKEN',
        '--validation-endpoint', str(endpoint_file), '--runtime-report', str(runtime)],
        env=env, cwd=folder, stdout=stdout, stderr=stderr)
    evidence = []
    try:
        deadline = time.monotonic() + 45
        while not endpoint_file.exists() and time.monotonic() < deadline:
            assert process.poll() is None, 'Host exited before endpoint'
            time.sleep(.02)
        endpoint = json.loads(endpoint_file.read_text())
        def call(op, passed=True, **parameters):
            response = exchange(endpoint, token, dict(op=op, **parameters))
            evidence.append(dict(op=op, response=response))
            assert response['passed'] == passed, response
            return response.get('value')
        def version():
            return json.loads(call('query', name='document.version'))
        def edit(command, parameters, base=None, passed=True):
            return call('edit', passed=passed, requestId='host-' + str(len(evidence)), commandId=command,
                        parameters=parameters, baseVersion=base or version())
        def event(action, **fields):
            call('input', event=dict(action=action, **fields))
        call('wait-frames', frames=20)
        descriptor = call('describe')
        assert any(d['id'] == 'node.transform' for d in descriptor['edits'])
        count = call('query', name='scene.nodeCount')
        original = version()
        edit('node.create', dict(id='observation-node'))
        assert call('query', name='scene.nodeCount') == count + 1
        edit('node.rename', dict(value='Rejected stale write'), base=original, passed=False)
        edit('history.undo', {})
        assert call('query', name='scene.nodeCount') == count
        edit('node.select', dict(id=node))
        before_drag = version()
        event('click', target='mode.Move');call('wait-frames', frames=3)
        event('mouse', target='gizmo.0', down=True);call('wait-frames', frames=3)
        event('mouse', target='gizmo.0', down=True, offset=[30, 0]);call('wait-frames', frames=4)
        event('mouse', target='gizmo.0', down=False, offset=[30, 0]);call('wait-frames', frames=3)
        assert version()['contentHash'] != before_drag['contentHash'], 'Input drag did not modify document'
        edit('node.select', dict(id=other))
        event('click', target='pick.' + node);call('wait-frames', frames=4)
        assert call('query', name='selection.id') == node
        call('screenshot', label='observation-' + workflow)
        call('wait-until', name='engine.screenshots', equals=1, timeoutMs=10000)
        image = folder / 'captures' / ('observation-' + workflow + '.png')
        assert image.is_file() and image.stat().st_size > 1000
        call('input', passed=False, event=dict(action='mouse', target='missing', down=True))
        call('query', passed=False, name='missing')
        call('assert', passed=False, name='engine.status', equals='missing')
        call('input', passed=False, domain='unknown', event=dict(action='focus', focused=True))
        call('input', passed=False, domain='game', event=dict(action='focus', focused=True))
        edit('preview.play', {})
        call('wait-until', name='editor.playing', equals=True, timeoutMs=10000)
        call('assert', name='engine.gameInputSource', equals='window')
        call('input', passed=False, domain='game', event=dict(action='key', key=999, down=True))
        call('assert', name='engine.gameInputSource', equals='window')
        call('input', domain='game', event=dict(action='focus', focused=True))
        before_input = call('query', name='engine.frameCount')
        call('input', domain='game', event=dict(action='key', key=87 if workflow == 'exploration' else 70, down=True))
        call('wait-frames', frames=25)
        after_input = call('query', name='engine.frameCount')
        call('input', domain='game', event=dict(action='key', key=87 if workflow == 'exploration' else 70, down=False))
        call('wait-frames', frames=3)
        edit('preview.stop', {})
        call('assert', name='editor.playing', equals=False)
        # Use the developer entry for control discovery under the same session token.
        result = subprocess.run([sys.executable, str(Path(__file__).with_name('azure.py')), 'describe', '--endpoint', str(endpoint_file),
                                 '--token-env', 'AZURE_OBSERVATION_TEST_TOKEN', '--json'], env=env, capture_output=True, text=True, timeout=10)
        assert result.returncode == 0, result.stderr
        process.wait(timeout=120)
        assert process.returncode == 0
    finally:
        if process.poll() is None:
            process.kill();process.wait()
        stdout.close();stderr.close()
        (folder / 'control-results.json').write_text(json.dumps(evidence, indent=2) + '\n', encoding='utf-8')
    logs = (folder / 'stdout.log').read_text() + (folder / 'stderr.log').read_text()
    assert 'VUID-' not in logs and 'Validation Error' not in logs, logs[-3000:]
    assert 'Allocator after unload: buffers=0 images=0' in logs
    data = json.loads(runtime.read_text(encoding='utf-8'))
    assert not data['editorWorkspace']['uiErrors']
    assert data['replayedActions'] == 3, data.get('replayedActions')
    route = [frame for frame in data['routeFrames'] if before_input <= frame['frame'] <= after_input]
    assert route and all(frame['focused'] for frame in route), 'Game replay focus overwritten by polling'
    if workflow == 'exploration':
        poses = [character['position'] for frame in route for character in frame['characters'] if character['node'] == 'hero:body']
        assert len(poses) > 2 and sum((a-b)**2 for a,b in zip(poses[0], poses[-1])) > .01, 'Game key did not move character'
    else:
        assert any(frame['interactionTarget'] == 'asset-b' for frame in route), 'Game key did not select asset'
    from PIL import Image
    assert Image.open(image).size == (1280, 720)
    return dict(workflow=workflow, status='passed', operations=len(evidence), physicalKeyboardMouse='not performed', mode='production with fixed frame step')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', required=True, type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='azure observation host ') as temporary:
        output = args.output.resolve() if args.output else Path(temporary)
        results = [run(args.executable.resolve(), output, workflow) for workflow in ('exploration', 'inspector')]
        (output / 'summary.json').write_text(json.dumps(dict(status='passed', workflows=results), indent=2) + '\n')
        print(json.dumps(results))
