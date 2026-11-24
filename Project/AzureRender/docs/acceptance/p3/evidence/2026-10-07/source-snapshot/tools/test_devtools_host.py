"""Exercise development services through production edits in two projects."""
import argparse
import json
import os
from pathlib import Path
import secrets
import shutil
import subprocess
import tempfile
import time
import ctypes
from ctypes import wintypes
from PIL import Image
from create_playable_project import create
from test_validation_protocol import exchange


def find_window(pid):
    windows=[]
    callback=ctypes.WINFUNCTYPE(wintypes.BOOL,wintypes.HWND,wintypes.LPARAM)
    @callback
    def visit(hwnd,_):
        owner=wintypes.DWORD()
        ctypes.windll.user32.GetWindowThreadProcessId(hwnd,ctypes.byref(owner))
        title=ctypes.create_unicode_buffer(256)
        ctypes.windll.user32.GetWindowTextW(hwnd,title,256)
        if owner.value==pid and title.value.startswith('AzureRender'): windows.append(hwnd)
        return True
    ctypes.windll.user32.EnumWindows(visit,0)
    return windows[0] if windows else None


def run(executable, output, workflow, config):
    root = Path(__file__).resolve().parents[1]
    folder = output / workflow
    folder.mkdir(parents=True)
    project = folder / 'Project with spaces'
    if workflow == 'exploration':
        create(project)
    else:
        shutil.copytree(root / 'assets_public/scene_inspector', project, ignore=shutil.ignore_patterns('.azure'))
    source = folder / 'shader sources'
    shutil.copytree(root / 'shaders', source)
    options = json.loads(config.read_text(encoding='utf-8'))
    options.update(sourceRoot=str(source), scratchRoot=str(folder / 'shader candidates'), pollIntervalMs=50)
    descriptor = folder / 'reload.json'
    descriptor.write_text(json.dumps(options), encoding='utf-8')
    endpoint_file, runtime = folder / 'endpoint.json', folder / 'runtime.json'
    token = secrets.token_hex(32)
    env = dict(os.environ, AZURE_DEVTOOLS_TEST_TOKEN=token, AZURERENDER_EDITOR_CONFIG=str(folder / 'config'),
               VK_LAYER_VALIDATE_SYNC='1')
    stdout = (folder / 'stdout.log').open('w', encoding='utf-8')
    stderr = (folder / 'stderr.log').open('w', encoding='utf-8')
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    process = subprocess.Popen([str(executable), '--editor-project', str(project / 'project.azureproject'),
        '--width', '1920', '--height', '1080', '--fixed-frame-step',
        '--resource-root', str(folder), '--shader-reload', str(descriptor),
        '--validation-token-env', 'AZURE_DEVTOOLS_TEST_TOKEN', '--validation-endpoint', str(endpoint_file),
        '--runtime-report', str(runtime)], cwd=folder, env=env, stdout=stdout, stderr=stderr, startupinfo=startup)
    evidence = []
    try:
        deadline = time.monotonic() + 45
        while not endpoint_file.exists() and time.monotonic() < deadline:
            assert process.poll() is None, 'Host exited before endpoint'
            time.sleep(.02)
        endpoint = json.loads(endpoint_file.read_text())
        def call(op, passed=True, **fields):
            response = exchange(endpoint, token, dict(op=op, **fields))
            evidence.append(dict(op=op, fields=fields, response=response))
            assert response['passed'] == passed, response
            return response.get('value')
        def edit(command, passed=True, **parameters):
            base = json.loads(call('query', name='document.version'))
            result = call('edit', passed=passed, requestId='devtools-' + str(len(evidence)),
                          commandId=command, parameters=parameters, baseVersion=base)
            return result['value'] if passed else None
        def frames(count=8):
            call('wait-frames', frames=count)
        def click(target):
            call('input', event=dict(action='click', target=target)); frames()
        def status():
            return json.loads(call('query', name='developer.status'))
        frames(15)
        original = call('query', name='document.version')
        handles = [edit('preview.create', renderer=name, extent=[192,128])['handle']
                   for name in ('character','sample','blackhole')]
        frames()
        views = [edit('preview.inspect', handle=handle) for handle in handles]
        assert len({view['colorImage'] for view in views}) == 3
        assert all(view['renderCount'] == 1 for view in views)
        edit('preview.create', passed=False, renderer='sample')
        edit('preview.capture', handle=handles[0], label='independent-camera')
        assert Image.open(folder / 'captures/independent-camera.png').size == (192,128)
        edit('preview.resize', handle=handles[0], extent=[320,180]);frames()
        assert edit('preview.inspect', handle=handles[0])['extent'] == [320,180]
        edit('preview.request', handle=handles[2]);frames()
        assert edit('preview.inspect', handle=handles[2])['renderCount'] == 2
        for handle in handles:
            edit('preview.release', handle=handle)
            edit('preview.request', passed=False, handle=handle)
        frames()
        # Content Browser grid uses the same thumbnail operation as tools.
        click('assets.grid');frames(20)
        current = status()
        thumbnails = current['thumbnails']
        assert thumbnails and len(thumbnails) <= 2, current
        # UUID sorting does not identify a particular geometry or material.
        # The foreground mask below belongs to the declared public guide fixture.
        same = edit('preview.asset', asset='guide' if workflow=='exploration' else thumbnails[0]['asset'])
        assert any(entry['handle']==same['handle'] for entry in thumbnails), 'The visible Grid must have requested the tested model'
        thumb = next(entry for entry in thumbnails if entry['handle']==same['handle'])
        assert same['handle'] == thumb['handle']
        edit('preview.release', passed=False, handle=thumb['handle'])
        frames(20)
        assert edit('preview.inspect', handle=thumb['handle'])['renderCount'] == 1
        edit('preview.capture', handle=thumb['handle'], label='asset-thumbnail')
        if workflow=='exploration':
            assert Path(same['path']).name=='guide.gltf', 'Framing mask requires the declared guide model'
            image=Image.open(folder/'captures/asset-thumbnail.png').convert('RGB')
            foreground=[(x,y) for y in range(image.height) for x in range(image.width)
                        if (lambda rgb: rgb[2]>100 and rgb[0]<50 and rgb[1]>80)(image.getpixel((x,y)))]
            assert foreground and min(y for _,y in foreground)>4 and max(y for _,y in foreground)<image.height-4, 'Thumbnail crops its public model'
        asset_path=Path(same['path'])
        content=json.loads(asset_path.read_text(encoding='utf-8'))
        content.setdefault('extras',{})['previewFingerprintFixture']=1
        asset_path.write_text(json.dumps(content),encoding='utf-8');frames(25)
        changed=next(entry for entry in status()['thumbnails'] if entry['asset']==thumb['asset'])
        assert changed['fingerprint']!=thumb['fingerprint'] and changed['handle']!=thumb['handle']
        edit('preview.inspect',passed=False,handle=thumb['handle'])
        click('assets.grid')
        edit('preview.clear');frames()
        # Camera preview is rendered through a real panel and stops when hidden.
        click('tool.capture')
        click('preview.camera');frames(15)
        camera = status()['camera'];assert camera and camera['renderCount'] > 1
        camera_handle=edit('preview.camera',enabled=True)['handle']
        edit('preview.release',passed=False,handle=camera_handle)
        edit('preview.resize',handle=camera_handle,extent=[400,200]);frames(15)
        assert edit('preview.inspect',handle=camera_handle)['extent']==[400,200]
        click('menu.View');click('panel.capture');frames()
        count = status()['camera']['renderCount'];frames(20)
        assert status()['camera']['renderCount'] == count, 'Hidden panel continues rendering'
        edit('preview.clear');frames()
        assert json.loads(call('query', name='document.version'))['contentHash'] == json.loads(original)['contentHash']
        old=edit('preview.create',renderer='sample')['handle'];frames()
        edit('node.create',id='preview-invalidation-fixture');frames()
        edit('preview.inspect',passed=False,handle=old);edit('history.undo');frames()
        assert json.loads(call('query', name='document.version'))['contentHash'] == json.loads(original)['contentHash']
        # Automatic source detection, compiler failure preservation and recovery.
        baseline = status()['shaderGeneration']
        source_file = source / 'mesh.frag'
        valid = source_file.read_text(encoding='utf-8')
        source_file.write_text(valid + '\ninvalid shader syntax\n', encoding='utf-8')
        call('wait-until', name='developer.shaderState', equals='Error', timeoutMs=60000)
        assert status()['shaderGeneration'] == baseline
        source_file.write_text(valid + '\n// accepted source dependency\n', encoding='utf-8')
        call('wait-until', name='developer.shaderState', equals='Idle', timeoutMs=60000)
        accepted = status()['shaderGeneration'];assert accepted > baseline
        edit('preview.inspect', passed=False, handle=handles[0])
        edit('developer.shader-rebuild');edit('developer.shader-cancel')
        call('wait-until', name='developer.shaderState', equals='Cancelled', timeoutMs=60000)
        assert status()['shaderGeneration'] == accepted
        old=edit('preview.create',renderer='sample')['handle'];frames()
        edit('preview.play');frames(20);edit('preview.inspect',passed=False,handle=old)
        edit('preview.stop');frames()
        window=find_window(process.pid);assert window
        user=ctypes.windll.user32
        user.MoveWindow.argtypes=[wintypes.HWND,ctypes.c_int,ctypes.c_int,ctypes.c_int,ctypes.c_int,wintypes.BOOL]
        user.ShowWindow.argtypes=[wintypes.HWND,ctypes.c_int]
        old=edit('preview.create',renderer='sample')['handle'];frames()
        user.MoveWindow(window,30,30,1440,900,True);frames(15)
        edit('preview.inspect',passed=False,handle=old)
        user.ShowWindow(window,6);time.sleep(.25);user.ShowWindow(window,9);frames(15)
        call('screenshot', label='r7-editor-' + workflow)
        call('wait-until', name='engine.screenshots', equals=1, timeoutMs=10000)
        # Closing the host while a compiler is owned must join and release it.
        edit('developer.shader-rebuild')
        ctypes.windll.user32.PostMessageW.argtypes=[wintypes.HWND,wintypes.UINT,wintypes.WPARAM,wintypes.LPARAM]
        ctypes.windll.user32.PostMessageW(window, 0x0010, 0, 0)
        process.wait(timeout=120)
    finally:
        if process.poll() is None:
            process.terminate();process.wait(timeout=5)
        stdout.close();stderr.close()
        (folder / 'control-results.json').write_text(json.dumps(evidence, indent=2) + '\n', encoding='utf-8')
    logs = (folder / 'stdout.log').read_text(encoding='utf-8') + (folder / 'stderr.log').read_text(encoding='utf-8')
    assert process.returncode == 0, logs[-5000:]
    assert not any(marker in logs.lower() for marker in ('vuid-', 'validation error', 'sync-hazard', 'hazard detected')), logs[-10000:]
    assert 'Allocator after unload: buffers=0 images=0' in logs
    report = json.loads(runtime.read_text(encoding='utf-8'))
    assert not report['editorWorkspace']['uiErrors']
    return dict(workflow=workflow, status='passed', operations=len(evidence), shaderGeneration=accepted,
                physicalKeyboardMouse='not performed', synchronizationValidation=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', required=True, type=Path)
    parser.add_argument('--shader-config', required=True, type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='azure devtools host ') as temporary:
        output = args.output.resolve() if args.output else Path(temporary)
        results = [run(args.executable.resolve(), output, workflow, args.shader_config.resolve())
                   for workflow in ('exploration','inspector')]
        (output / 'summary.json').write_text(json.dumps(dict(status='passed', workflows=results), indent=2) + '\n', encoding='utf-8')
        print(json.dumps(results))
