"""Verify typed setting sources and real settings controls in two editor projects."""
import argparse
import json
import os
from pathlib import Path
import secrets
import shutil
import subprocess
import tempfile
import time
from create_playable_project import create
from test_validation_protocol import exchange


def run(executable, output, workflow):
    folder = output / workflow
    folder.mkdir(parents=True)
    project = folder / 'Project with spaces'
    if workflow == 'exploration':
        create(project)
    else:
        shutil.copytree(Path(__file__).resolve().parents[1] / 'assets_public/scene_inspector', project,
                        ignore=shutil.ignore_patterns('.azure'))
    config = folder / 'config'
    config.mkdir()
    user = config / 'settings.json'
    user.write_text(json.dumps(dict(schemaVersion=1, values={'render.exposure': .5})), encoding='utf-8')
    endpoint_file, runtime = folder / 'endpoint.json', folder / 'runtime.json'
    token = secrets.token_hex(32)
    env = dict(os.environ, AZURE_SETTINGS_TEST_TOKEN=token, AZURERENDER_EDITOR_CONFIG=str(config))
    stdout = (folder / 'stdout.log').open('w', encoding='utf-8')
    stderr = (folder / 'stderr.log').open('w', encoding='utf-8')
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    process = subprocess.Popen([str(executable), '--editor-project', str(project / 'project.azureproject'),
        '--width', '1920', '--height', '1080', '--smoke-frames', '1200', '--fixed-frame-step',
        '--resource-root', str(folder), '--set', 'render.exposure=1.25',
        '--validation-token-env', 'AZURE_SETTINGS_TEST_TOKEN', '--validation-endpoint', str(endpoint_file),
        '--runtime-report', str(runtime)], cwd=folder, env=env, stdout=stdout, stderr=stderr, startupinfo=startup)
    evidence = []
    try:
        deadline = time.monotonic() + 45
        while not endpoint_file.exists() and time.monotonic() < deadline:
            assert process.poll() is None, 'Host exited before settings endpoint'
            time.sleep(.02)
        endpoint = json.loads(endpoint_file.read_text())
        def call(op, passed=True, **fields):
            response = exchange(endpoint, token, dict(op=op, **fields))
            evidence.append(dict(op=op, fields=fields, response=response))
            assert response['passed'] == passed, response
            return response.get('value')
        def edit(command, passed=True, **parameters):
            base = json.loads(call('query', name='document.version'))
            return call('edit', passed=passed, requestId='settings-' + str(len(evidence)),
                        commandId=command, parameters=parameters, baseVersion=base)
        def event(action, **fields):
            call('input', event=dict(action=action, **fields))
        def wait(name, value):
            call('wait-until', name=name, equals=value)
        def frames():
            call('wait-frames', frames=5)
        frames()
        original = call('query', name='document.version')
        wait('render.exposure', 1.25)
        assert call('query', name='setting.source.render.exposure') == 'command-line'
        edit('settings.set', name='render.exposure', value=2.0)
        wait('render.exposure', 2.0)
        assert call('query', name='document.version') == original, 'Temporary render override changed the authored document'
        assert call('query', name='setting.source.render.exposure') == 'console'
        edit('settings.set', passed=False, name='render.exposure', value=20.0)
        wait('render.exposure', 2.0)
        edit('settings.reset', name='render.exposure')
        wait('render.exposure', 1.25)
        edit('settings.set', name='editor.scale', value=1.0, source='user-file')
        frames()
        event('click', target='tool.settings')
        frames()
        event('click', target='settings.search')
        event('text', text='diagnostics.verbose')
        frames()
        event('click', target='setting.diagnostics.verbose')
        wait('setting.diagnostics.verbose', True)
        assert call('query', name='setting.source.diagnostics.verbose') == 'console'
        event('click', target='settings.save')
        frames()
        saved = json.loads(user.read_text(encoding='utf-8'))
        assert saved['values']['editor.scale'] == 1.0
        assert saved['values']['render.exposure'] == .5, 'Temporary override leaked into user preferences'
        assert call('query', name='document.version') == original, 'Settings changed project content'
        call('screenshot', label='u4-settings-' + workflow)
        wait('engine.screenshots', 1)
        process.wait(timeout=120)
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=5)
        stdout.close()
        stderr.close()
    assert process.returncode == 0, (folder / 'stderr.log').read_text(encoding='utf-8')
    assert 'VUID-' not in (folder / 'stderr.log').read_text(encoding='utf-8')
    assert 'Allocator after unload: buffers=0 images=0' in (folder / 'stdout.log').read_text(encoding='utf-8')
    data = json.loads(runtime.read_text(encoding='utf-8'))
    workspace = data['editorWorkspace']
    assert workspace['uiErrors'] == []
    assert workspace['panels']['settings']['open']
    assert workspace['settings']['diagnostics.verbose']['value'] is True
    diagnostic_rows = [json.loads(line) for line in (folder / 'captures/azurerender.log.jsonl').read_text(encoding='utf-8').splitlines()]
    assert any('Applied setting layers:' in row['message'] for row in diagnostic_rows), 'Verbose diagnostics did not consume the setting'
    screenshots = list((folder / 'captures').glob('u4-settings-*.png'))
    assert len(screenshots) == 1, 'Production screenshot missing'
    shutil.copy2(screenshots[0], folder / 'settings.png')
    (folder / 'control-results.json').write_text(json.dumps(evidence, indent=2) + '\n', encoding='utf-8')
    # Reopen the same editor preferences in a fresh host.
    reopened = folder / 'reopened.json'
    result = subprocess.run([str(executable), '--editor-project', str(project / 'project.azureproject'),
        '--width', '1920', '--height', '1080', '--smoke-frames', '20', '--runtime-report', str(reopened)],
        cwd=folder, env=env, startupinfo=startup, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=120)
    assert result.returncode == 0, result.stdout + result.stderr
    restored = json.loads(reopened.read_text(encoding='utf-8'))['editorWorkspace']
    assert restored['panels']['settings']['open'] and restored['settings']['render.exposure']['value'] == .5
    assert restored['settings']['render.exposure']['source'] == 'user-file'
    return dict(workflow=workflow, operations=len(evidence), realCheckbox=True, sources=True,
                reset=True, invalidRejected=True, documentPreserved=True, persistence=True, reopen=True)


def main(executable, output):
    output.mkdir(parents=True)
    rows = [run(executable.resolve(), output.resolve(), name) for name in ('exploration', 'inspector')]
    result = dict(schemaVersion=1, status='passed', workflows=rows, physicalKeyboardMouse='not performed')
    (output / 'summary.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(result))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if args.output:
        main(args.executable, args.output)
    else:
        with tempfile.TemporaryDirectory(prefix='azure settings host ') as temporary:
            main(args.executable, Path(temporary) / 'evidence')
