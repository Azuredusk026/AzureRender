"""Validate procedural content through production edits, proposals and GPU previews."""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import secrets
import shutil
import subprocess
import sys
import time
from PIL import Image, ImageStat
from create_playable_project import create
from test_validation_protocol import exchange
from test_proposal_host import close_host

ROOT = Path(__file__).resolve().parents[1]


def run(executable, output, workflow):
    folder = output / workflow
    folder.mkdir(parents=True)
    project = folder / 'Procedural Project with spaces'
    if workflow == 'exploration':
        create(project)
    else:
        shutil.copytree(ROOT / 'assets_public/scene_inspector', project,
                        ignore=shutil.ignore_patterns('.azure'))
    fixture = folder / 'fixed-model-response.json'
    fixture.write_text(json.dumps({'responses': []}), encoding='utf-8')
    endpoint_file, runtime = folder / 'endpoint.json', folder / 'runtime.json'
    token = secrets.token_hex(32)
    env = dict(os.environ, AZURE_PROCEDURAL_TOKEN=token, VK_LAYER_VALIDATE_SYNC='1',
               VK_INSTANCE_LAYERS='VK_LAYER_KHRONOS_validation', VK_LOADER_DEBUG='error,layer',
               AZURERENDER_EDITOR_CONFIG=str(folder / 'config'))
    logs = [(folder / name).open('w', encoding='utf-8') for name in ('stdout.log', 'stderr.log')]
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    process = subprocess.Popen([str(executable), '--editor-project', str(project / 'project.azureproject'),
        '--width', '1920', '--height', '1080', '--fixed-frame-step', '--resource-root', str(folder),
        '--set', 'ai.enabled=true', '--ai-python', sys.executable, '--ai-fixture', str(fixture),
        '--validation-token-env', 'AZURE_PROCEDURAL_TOKEN', '--validation-endpoint', str(endpoint_file),
        '--runtime-report', str(runtime)], cwd=folder, env=env, stdout=logs[0], stderr=logs[1], startupinfo=startup)
    evidence, costs = [], []
    try:
        deadline = time.monotonic() + 45
        while not endpoint_file.exists() and time.monotonic() < deadline:
            assert process.poll() is None, 'Host exited before endpoint'
            time.sleep(.02)
        endpoint = json.loads(endpoint_file.read_text(encoding='utf-8'))

        def call(op, passed=True, **fields):
            response = exchange(endpoint, token, dict(op=op, timeoutMs=60000, **fields))
            evidence.append(dict(op=op, fields=fields, response=response))
            assert response['passed'] == passed, response
            return response.get('value')

        def query(name):
            return call('query', name=name)

        def edit(command, passed=True, **parameters):
            result = call('edit', passed=passed, requestId='procedural-' + str(len(evidence)),
                commandId=command, parameters=parameters, baseVersion=json.loads(query('document.version')))
            return result['value'] if passed else None

        def frames(count=8):
            call('wait-frames', frames=count)

        def generate(generator, name, source, **parameters):
            before = time.perf_counter()
            identity = edit('asset.generate', generator=generator, output='assets:/' + name,
                parameters=dict(source=source, **parameters), license='CC0-1.0 public procedural fixture')
            costs.append(dict(generator=generator, asset=name, milliseconds=(time.perf_counter()-before)*1000,
                bytes=(project / 'assets' / name).stat().st_size))
            return identity

        def capture(identity, label):
            edit('preview.clear')
            handle = edit('preview.asset', asset=identity)['handle']
            frames(16)
            result = edit('preview.capture', handle=handle, label=label)
            image = Image.open(result['path']).convert('RGB')
            assert max(ImageStat.Stat(image).stddev) > 4, 'Model preview must contain visible geometry'
            return hashlib.sha256(Path(result['path']).read_bytes()).hexdigest()

        frames()
        source = (ROOT / 'assets_public/procedural/modular-solid.json').read_text(encoding='utf-8')
        parameters = json.loads((ROOT / ('assets_public/procedural/' +
            ('building-parameters.json' if workflow == 'exploration' else 'prop-parameters.json'))).read_text())
        model = generate('azure.procedural', 'parameterized.gltf', source, **parameters)
        first = capture(model, 'parameterized-model')
        generated = project / 'assets/parameterized.gltf'
        valid_bytes, valid_meta = generated.read_bytes(), Path(str(generated) + '.azmeta').read_bytes()
        edit('asset.generate', passed=False, generator='azure.procedural', output='assets:/parameterized.gltf',
            parameters={'source': '{broken'}, license='CC0-1.0')
        frames()
        assert generated.read_bytes() == valid_bytes and Path(str(generated) + '.azmeta').read_bytes() == valid_meta, 'Failed generation must preserve content and metadata'
        assert capture(model, 'valid-after-failure') == first, 'Failed generation must preserve a valid rendered asset'
        model_meta = json.loads(valid_meta)
        assert model_meta['generation']['generatorId'] == 'azure.procedural'
        assert model_meta['generation']['generatorVersion'] == 1
        # Direct import provides an independent cost and cache comparison.
        copy = folder / 'direct-model.gltf'
        copy.write_bytes(valid_bytes)
        before = time.perf_counter()
        imported = edit('asset.import', path=str(copy))
        costs.append(dict(generator='direct-import', milliseconds=(time.perf_counter()-before)*1000, bytes=len(valid_bytes)))
        assert capture(imported, 'direct-import') == first, 'Imported and generated geometry must render identically'
        rig_source = (ROOT / 'assets_public/procedural/rig/articulated-tool.json').read_text(encoding='utf-8')
        rig = generate('azure.rig-text', 'articulated.gltf', rig_source, scale=1.2)
        graph = generate('azure.rig-animation-graph', 'articulated-graph.json', rig_source, scale=1.2)
        capture(rig, 'rig-bind-pose')
        for i in range(2):
            edit('node.place', resource=rig, id='procedural-rig-' + str(i))
            edit('node.transform', translation=[i*2, 0, -2])
            edit('component.add', type='azure.animator')
            edit('component.field', type='azure.animator', field='state', value='anim_sweep')
            edit('component.field', type='azure.animator', field='asset', value=graph)
            edit('component.field', type='azure.animator', field='startTime', value=i*.25)
        candidate = dict(schemaVersion=1, operations=[dict(command='asset.generate', parameters=dict(
            generator='azure.procedural', output='assets:/model-proposal.gltf', license='CC0-1.0',
            parameters=dict(source=source, **parameters)))])
        fixture.write_text(json.dumps(dict(responses=[candidate])), encoding='utf-8')
        before_version = query('document.version')
        edit('ai.generate', runId='procedural-model', domain='asset-parameters', target='document', instruction='Generate the declared parametric model')
        call('wait-until', name='ai.state', equals='Ready')
        assert query('document.version') == before_version and not (project/'assets/model-proposal.gltf').exists()
        edit('ai.apply')
        call('wait-until', name='ai.state', equals='Applied')
        assert (project/'assets/model-proposal.gltf').read_bytes() == valid_bytes
        candidate['operations'][0]['parameters'].update(generator='azure.rig-text', output='assets:/rig-proposal.gltf', parameters=dict(source=rig_source, scale=1.2))
        fixture.write_text(json.dumps(dict(responses=[candidate])), encoding='utf-8')
        edit('ai.generate', runId='procedural-rig', domain='asset-parameters', target='document', instruction='Generate the declared rigid action asset')
        call('wait-until', name='ai.state', equals='Ready')
        edit('ai.apply')
        call('wait-until', name='ai.state', equals='Applied')
        assert (project/'assets/rig-proposal.gltf').read_bytes() == (project/'assets/articulated.gltf').read_bytes()
        edit('document.save')
        project_data = json.loads((project/'project.azureproject').read_text(encoding='utf-8'))
        level_path = project/'assets'/project_data['startupScene'].removeprefix('assets:/')
        saved_level = level_path.read_bytes()
        edit('document.reload')
        edit('document.save')
        assert level_path.read_bytes() == saved_level, 'Generated resources and all authored fields survive save and reopen'
        # Prefab expansion appends instances after authored nodes on load.
        # The runtime array order can therefore differ from the pre-load order.
        saved = query('document.contentHash')
        edit('preview.play')
        frames(90)
        edit('preview.stop')
        frames()
        assert query('document.contentHash') == saved, 'Preview stop restores the authored document'
        edit('preview.play')
        frames(60)
        call('screenshot', label='procedural-live-rigs')
        call('wait-until', name='engine.screenshots', equals=1)
        close_host(process)
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=5)
        for log in logs:
            log.close()
        (folder/'control-results.json').write_text(json.dumps(evidence, indent=2), encoding='utf-8')
    assert process.returncode == 0, (folder/'stderr.log').read_text(encoding='utf-8')
    validation_logs = '\n'.join((folder/name).read_text(encoding='utf-8') for name in ('stdout.log', 'stderr.log'))
    assert re.search(r'Insert.*VK_LAYER_KHRONOS_validation', validation_logs, re.I), 'The real Vulkan validation layer must be inserted'
    assert not any(marker in validation_logs.lower() for marker in ('vuid-', 'validation error', 'sync-hazard', 'hazard detected'))
    assert 'Allocator after unload: buffers=0 images=0' in (folder/'stdout.log').read_text(encoding='utf-8')
    data = json.loads(runtime.read_text(encoding='utf-8'))
    animations = [row for row in data['animations'] if row['node'].startswith('procedural-rig-')]
    assert len(animations) == 2 and abs(animations[0]['time']-animations[1]['time']) > .1, 'Real instances must advance separate animation clocks'
    result = dict(workflow=workflow, status='passed', operations=len(evidence), generationCosts=costs,
                  animations=animations, synchronizedValidation=True, physicalKeyboardMouse='not performed')
    (folder/'result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    return result


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    run_output = output / ('run-' + str(time.time_ns()))
    rows = [run(args.executable.resolve(), run_output, workflow) for workflow in ('exploration', 'inspector')]
    (args.output/'summary.json').write_text(json.dumps(dict(status='passed', workflows=rows), indent=2), encoding='utf-8')
    print(json.dumps(rows))


if __name__ == '__main__':
    main()
