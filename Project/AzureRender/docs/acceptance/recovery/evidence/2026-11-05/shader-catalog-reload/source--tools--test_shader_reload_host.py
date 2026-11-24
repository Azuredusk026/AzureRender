"""Verify source recovery and owned compiler shutdown through the editor host."""
import argparse
import ctypes
import json
import os
from pathlib import Path
import secrets
import shutil
import subprocess
import tempfile
import time

from create_playable_project import create
from test_devtools_host import find_window
from test_validation_protocol import exchange


def run(executable, config, output, workflow):
    engine = Path(__file__).resolve().parents[1]
    folder = output / workflow
    folder.mkdir(parents=True)
    project = folder / 'Project with spaces'
    if workflow == 'exploration':
        create(project)
    else:
        shutil.copytree(engine / 'assets_public/scene_inspector', project,
                        ignore=shutil.ignore_patterns('.azure'))
    source = folder / 'source'
    shutil.copytree(engine / 'shaders', source)
    descriptor = json.loads(config.read_bytes())
    descriptor.update(sourceRoot=str(source), scratchRoot=str(folder / 'candidates'),
                      pollIntervalMs=50)
    reload_file = folder / 'reload.json'
    reload_file.write_text(json.dumps(descriptor), encoding='utf-8')
    token = secrets.token_hex(32)
    env = dict(os.environ, AZURE_SHADER_HOST_TOKEN=token,
               AZURERENDER_EDITOR_CONFIG=str(folder / 'config'), VK_LAYER_VALIDATE_SYNC='1')
    endpoint_file = folder / 'endpoint.json'
    runtime = folder / 'runtime.json'
    command = [str(executable), '--editor-project', str(project / 'project.azureproject'),
               '--editor-close-policy', 'save', '--width', '960', '--height', '540',
               '--fixed-frame-step', '--resource-root', str(folder),
               '--shader-reload', str(reload_file), '--validation-token-env',
               'AZURE_SHADER_HOST_TOKEN', '--validation-endpoint', str(endpoint_file),
               '--runtime-report', str(runtime)]
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    evidence = []
    timings = {}
    with (folder / 'stdout.log').open('w', encoding='utf-8') as stdout, \
            (folder / 'stderr.log').open('w', encoding='utf-8') as stderr:
        process = subprocess.Popen(command, cwd=folder, env=env, stdout=stdout,
                                   stderr=stderr, startupinfo=startup)
        try:
            deadline = time.monotonic() + 45
            while not endpoint_file.exists() and time.monotonic() < deadline:
                assert process.poll() is None, 'Host exited before endpoint'
                time.sleep(.02)
            endpoint = json.loads(endpoint_file.read_bytes())

            def call(op, **fields):
                response = exchange(endpoint, token, dict(op=op, **fields))
                evidence.append(dict(op=op, fields=fields, response=response))
                assert response['passed'], response
                return response.get('value')

            def state():
                return json.loads(call('query', name='developer.status'))

            def wait(value):
                start = time.monotonic()
                call('wait-until', name='developer.shaderState', equals=value, timeoutMs=60000)
                timings[value] = time.monotonic() - start

            def edit(command_id):
                version = json.loads(call('query', name='document.version'))
                return call('edit', requestId='shader-' + str(len(evidence)),
                            commandId=command_id, parameters={}, baseVersion=version)

            call('wait-frames', frames=5, timeoutMs=10000)
            baseline = state()['shaderGeneration']
            path = source / 'mesh.frag'
            valid = path.read_bytes()
            path.write_bytes(valid + b'\ninvalid shader syntax\n')
            wait('Error')
            assert state()['shaderGeneration'] == baseline
            path.write_bytes(valid + b'\n// accepted source dependency\n')
            wait('Idle')
            accepted = state()['shaderGeneration']
            assert accepted > baseline
            candidate = folder / 'candidates' / str(accepted) / 'binary'
            for program in descriptor['programs']:
                assert (candidate / program['output']).read_bytes() == \
                    (Path(descriptor['binaryRoot']) / program['output']).read_bytes(), program
            edit('developer.shader-rebuild')
            edit('developer.shader-cancel')
            wait('Cancelled')
            assert state()['shaderGeneration'] == accepted
            call('screenshot', label='shader-recovered')
            call('wait-until', name='engine.screenshots', equals=1, timeoutMs=10000)
            edit('developer.shader-rebuild')
            window = find_window(process.pid)
            assert window
            user = ctypes.windll.user32
            user.PostMessageW.argtypes = [ctypes.c_void_p, ctypes.c_uint,
                                         ctypes.c_size_t, ctypes.c_ssize_t]
            user.PostMessageW(window, 0x0010, 0, 0)
            process.wait(timeout=120)
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=5)
            (folder / 'operations.json').write_text(json.dumps(evidence, indent=2), encoding='utf-8')
    logs = (folder / 'stdout.log').read_text(encoding='utf-8') + \
        (folder / 'stderr.log').read_text(encoding='utf-8')
    assert process.returncode == 0, logs[-4000:]
    assert not any(v in logs.lower() for v in ['vuid-', 'validation error', 'sync-hazard'])
    assert 'Allocator after unload: buffers=0 images=0' in logs
    assert not json.loads(runtime.read_bytes())['editorWorkspace']['uiErrors']
    return dict(workflow=workflow, status='passed', programs=len(descriptor['programs']),
                activeGeneration=accepted, outputBytesExact=True, timings=timings,
                physicalKeyboardMouse='not performed', command=command)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--config', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='azure shader host ') as temporary:
        output = args.output.resolve() if args.output else Path(temporary)
        cases = [run(args.executable.resolve(), args.config.resolve(), output, workflow)
                 for workflow in ['exploration', 'inspector']]
        result = dict(status='passed', cases=cases)
        (output / 'results.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
        print(json.dumps(result))
