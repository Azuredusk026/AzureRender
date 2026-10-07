"""Verify native close requests and real Save/Discard/Cancel controls."""
import argparse
import json
import os
from pathlib import Path
import secrets
import shutil
import subprocess
import time
from create_playable_project import create
from run_playable_long_run import NativeWindow
from test_validation_protocol import exchange


def run(executable, output, workflow, decision):
    folder = output / (workflow + '-' + decision)
    folder.mkdir(parents=True)
    project = folder / 'project'
    if workflow == 'exploration':
        create(project)
    else:
        shutil.copytree(Path(__file__).resolve().parents[1] / 'assets_public/scene_inspector', project,
                        ignore=shutil.ignore_patterns('.azure'))
    project_json = json.loads((project / 'project.azureproject').read_text(encoding='utf-8'))
    level = project / 'assets' / project_json['startupScene'].removeprefix('assets:/')
    saved = level.read_bytes()
    token = secrets.token_hex(32)
    endpoint_file = folder / 'endpoint.json'
    env = dict(os.environ, AZURE_UX_TOKEN=token, AZURERENDER_EDITOR_CONFIG=str(folder / 'config'))
    logs = [(folder / name).open('w', encoding='utf-8') for name in ('stdout.log', 'stderr.log')]
    process = subprocess.Popen([str(executable), '--editor-project', str(project / 'project.azureproject'),
        '--editor-close-policy', 'ask', '--width', '1280', '--height', '720', '--fixed-frame-step',
        '--validation-token-env', 'AZURE_UX_TOKEN', '--validation-endpoint', str(endpoint_file),
        '--runtime-report', str(folder / 'runtime.json')], env=env, stdout=logs[0], stderr=logs[1])
    evidence = []
    try:
        deadline = time.monotonic() + 45
        while not endpoint_file.exists() and time.monotonic() < deadline:
            assert process.poll() is None
            time.sleep(.02)
        endpoint = json.loads(endpoint_file.read_text(encoding='utf-8'))
        def call(op, **parameters):
            response = exchange(endpoint, token, dict(op=op, **parameters))
            evidence.append(dict(op=op, parameters=parameters, response=response))
            assert response['passed'], response
            return response.get('value')
        def workspace():
            return json.loads(call('query', name='editor.workspace'))
        def edit(command, **parameters):
            version = json.loads(call('query', name='document.version'))
            return call('edit', requestId='ux-' + str(len(evidence)), commandId=command,
                        parameters=parameters, baseVersion=version)
        def click(target):
            call('input', event=dict(action='click', target=target))
        call('wait-frames', frames=20)
        edit('node.rename', value='Guard authored value')
        authored = call('query', name='document.contentHash')
        selection = call('query', name='selection.id')
        window = NativeWindow(process.pid)
        assert window.find(), "Native editor window must be identified before posting close"
        window.hide()
        window.close()
        call('wait-frames', frames=6)
        assert process.poll() is None and workspace()['documentGuard'] == 1
        click('document.cancel')
        call('wait-frames', frames=5)
        assert workspace()['documentGuard'] == 0
        call('wait-frames', frames=180)
        live=workspace()
        assert 'history' not in live and len(json.dumps(live).encode('utf-8'))<=65536, 'Live workspace queries must keep bounded current state'
        assert call('query', name='document.contentHash') == authored
        assert call('query', name='selection.id') == selection
        assert level.read_bytes() == saved
        if decision == 'failure':
            backup = level.with_suffix('.guard-backup')
            level.rename(backup)
            level.mkdir()
            window.close()
            call('wait-frames', frames=5)
            click('document.save')
            call('wait-frames', frames=5)
            assert workspace()['documentGuard'] == 3 and process.poll() is None
            assert call('query', name='document.contentHash') == authored
            assert call('query', name='selection.id') == selection
            assert call('query', name='editor.dirty') is True
            click('document.cancel')
            call('wait-frames', frames=5)
            level.rmdir()
            backup.rename(level)
            decision = 'discard'
        # Reload uses the same confirmation controls.
        edit('document.reload')
        call('wait-frames', frames=5)
        assert workspace()['documentGuard'] == 1
        click('document.cancel')
        call('wait-frames', frames=5)
        assert call('query', name='document.contentHash') == authored
        window.close()
        call('wait-frames', frames=5)
        click('document.' + decision)
        assert process.wait(timeout=30) == 0
        assert (level.read_bytes() != saved) == (decision == 'save')
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=5)
        for log in logs:
            log.close()
        (folder / 'control-results.json').write_text(json.dumps(evidence, indent=2), encoding='utf-8')
    messages = (folder / 'stderr.log').read_text(encoding='utf-8')
    assert 'VUID-' not in messages and 'SYNC-HAZARD' not in messages
    return dict(workflow=workflow, decision=folder.name, passed=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output=args.output/("run-"+str(time.time_ns()))
    args.output.mkdir(parents=True, exist_ok=True)
    results = [run(args.executable.resolve(), args.output.resolve(), workflow, decision)
               for workflow in ('exploration', 'inspector') for decision in ('save', 'discard', 'failure')]
    (args.output / 'summary.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
    print(json.dumps(results))
