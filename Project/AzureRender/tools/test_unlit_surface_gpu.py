"""Check public unlit surfaces through Player, lighting and alpha composition."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile

import numpy as np
from PIL import Image
from create_visibility_fixture import surface, component


def linear(rgb):
    value = rgb.astype(np.float64) / 255
    return np.where(value <= .04045, value / 12.92, ((value + .055) / 1.055) ** 2.4)


def run(player, root, expected_validation='any', legacy_lit_red=False):
    root.mkdir(parents=True, exist_ok=False)
    def fingerprints():
        inputs = [player.resolve(), Path(__file__).resolve()]
        inputs += sorted((player.parent / 'shaders').glob('*.spv'))
        return {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in inputs}
    frozen_inputs = fingerprints()
    (root / 'input-fingerprints.json').write_text(json.dumps(frozen_inputs, indent=2))
    assets = root / 'assets'
    assets.mkdir()
    surface(assets / 'surface.gltf', double_sided=True)
    template = json.loads((assets / 'surface.gltf').read_text())
    cell = struct.pack('<16f', 0, 0, 0, .2, 0, 0, 0, .1,
                       0, 0, 0, .3, 1, 0, 0, 0)
    (assets / 'field.aziv').write_bytes(
        struct.pack('<8s4f3I', b'AZIV0001', -4, -4, -4, 8, 2, 2, 2) + cell * 8)
    (assets / 'probe.azenv').write_bytes(
        b'AZENV001' + struct.pack('<III', 2, 1, 1)
        + struct.pack('<8e', .4, .3, .2, 1, .4, .3, .2, 1))
    project = root / 'project.azureproject'
    project.write_text(json.dumps({'schemaVersion': 1, 'id': 'unlit-contract',
        'name': 'Unlit surface contract', 'mounts': [{'name': 'assets', 'path': 'assets'}],
        'startupScene': 'assets:/scene.azurelevel'}))
    frames, records, failures, metrics = {}, [], [], {}
    base = np.array([.12, .25, .35])
    emission = np.array([.04, .02, .01])
    receiver = np.array([.3, .16, .08])

    def capture(name, *, lighting=False, indirect=False, gi=False, probe=False, alpha=1, opacity=1,
                fixed=False, scene=True, unlit=True, isolation='beauty',
                emission_only=False, behind=False):
        model = copy.deepcopy(template)
        material = model['materials'][0]
        material['pbrMetallicRoughness'].update(
            baseColorFactor=[*(np.zeros(3) if emission_only else base), alpha],
            metallicFactor=0, roughnessFactor=.6)
        material['emissiveFactor'] = list(emission)
        material['alphaMode'] = 'BLEND' if alpha < 1 or opacity < 1 or behind else 'OPAQUE'
        profile = {'schemaVersion': 2, 'class': 'emissive' if unlit else 'generic',
                   'features': ['emissive-mask']}
        if unlit and not legacy_lit_red:
            profile['features'].append('unlit-surface')
            profile['surface'] = {'schemaVersion': 1, 'lighting': 'unlit'}
        material['extras'] = {'azureRenderMaterial': profile}
        model['extensionsUsed'] = ['AZURE_node_opacity']
        model['nodes'][0]['extensions'] = {'AZURE_node_opacity': {'schemaVersion': 1, 'opacity': opacity}}
        (assets / 'surface.gltf').write_text(json.dumps(model))
        rear = copy.deepcopy(template)
        rear['materials'][0]['pbrMetallicRoughness'].update(baseColorFactor=[*receiver, 1])
        rear['materials'][0]['extras'] = {'azureRenderMaterial': {
            'schemaVersion': 2, 'class': 'overlay', 'features': ['overlay']}}
        (assets / 'rear.gltf').write_text(json.dumps(rear))
        nodes = [
            {'id': 'rear', 'resourceId': 'rear', 'components': {
                'azure.transform': component('azure.transform', {'translation': [0, 0, -1], 'scale': [4, 4, 4]})}},
            {'id': 'surface', 'resourceId': 'surface', 'components': {
                'azure.transform': component('azure.transform', {'translation': [0, 0, -2 if behind else 0]})}},
            {'id': 'camera', 'components': {
                'azure.transform': component('azure.transform', {'translation': [0, 0, 3]}),
                'azure.camera': component('azure.camera', {'enabled': True, 'priority': 1,
                    'fieldOfView': 55, 'nearPlane': .1, 'farPlane': 20})}},
            {'id': 'light', 'components': {
                'azure.transform': component('azure.transform', {'translation': [0, 0, 2]})}}]
        level = {'schemaVersion': 1, 'id': name, 'sceneType': 'character',
            'resources': [{'id': 'rear', 'asset': 'assets:/rear.gltf'},
                          {'id': 'surface', 'asset': 'assets:/surface.gltf'}],
            'nodes': nodes, 'lights': [{'id': 'light', 'nodeId': 'light',
                'color': [1, 1, 1], 'intensity': 12, 'radius': 10, 'enabled': lighting}],
            'renderSettings': {'sceneLighting': scene, 'skyIntensity': 0,
                'cameraNear': .1, 'cameraFar': 20, 'shadowDistance': 20,
                'platform': False, 'silhouetteOutline': False, 'innerOutline': False,
                'stylized': True, 'bloomEnabled': False, 'antiAliasing': 0,
                'exposure': 0, 'toneMapping': False, 'contrast': 1, 'saturation': 1}}
        if indirect or gi:
            level['irradianceVolume'] = {'version': 1, 'asset': 'assets:/field.aziv',
                                         'enabled': True, 'intensity': 1}
        if indirect or probe:
            level['reflectionProbe'] = {'version': 1, 'texture': 'assets:/probe.azenv',
                'center': [0, 0, 0], 'halfExtents': [4, 4, 4], 'capturePosition': [0, 0, 0],
                'intensity': 1, 'blendDistance': 0, 'boxProjection': False}
        (assets / 'scene.azurelevel').write_text(json.dumps(level))
        output = root / name
        output.mkdir()
        command = [str(player.resolve()), '--project', str(project.resolve()),
            '--width', '128', '--height', '128', '--capture-dir', str((output / 'frames').resolve()),
            '--capture-frames', '1', '--fixed-frame-step', '--qa-camera', 'scene',
            '--qa-light', 'scene', '--qa-isolation', isolation,
            '--runtime-report', str((output / 'runtime.json').resolve())]
        if fixed:
            command += ['--disable-bindless', '--disable-compute-skinning', '--disable-parallel-recording']
        (output / 'command.json').write_text(json.dumps(command, indent=2))
        result = subprocess.run(command, capture_output=True, encoding='utf8', errors='replace', timeout=120)
        log = result.stdout + result.stderr
        (output / 'player.log').write_text(log, encoding='utf8')
        if result.returncode:
            (root / 'failure.json').write_text(json.dumps({'case': name, 'exitCode': result.returncode,
                'playerSha256': hashlib.sha256(player.read_bytes()).hexdigest(),
                'log': str(output / 'player.log')}, indent=2))
        assert result.returncode == 0, (name, 'Player failed; see player.log')
        assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]', log), name
        validated = 'Validation layer: enabled' in log
        if expected_validation != 'any':
            assert validated == (expected_validation == 'enabled'), (name, 'validation configuration')
        assert 'Allocator after unload: buffers=0 images=0' in log, name
        report = json.loads((output / 'runtime.json').read_text())
        assert not report['scriptErrors'] and not report['presentationErrors'], name
        manifest = json.loads((output / 'frames/capture_manifest.json').read_text())
        assert manifest['qaIsolation'] == isolation and manifest['qaCamera'] == 'scene', name
        assert np.allclose(manifest['qaCameraPosition'], [0, 0, 3], atol=1e-6), name
        frame = output / 'frames/frame_000000.png'
        frames[name] = np.asarray(Image.open(frame).convert('RGB'), dtype=np.int16)
        records.append({'name': name, 'imageSha256': hashlib.sha256(frame.read_bytes()).hexdigest(),
            'modelSha256': hashlib.sha256((assets / 'surface.gltf').read_bytes()).hexdigest(),
            'levelSha256': hashlib.sha256((assets / 'scene.azurelevel').read_bytes()).hexdigest(),
            'validationEnabled': validated})
        return frames[name][54:74, 54:74]

    cases = [('dark', {}), ('direct', {'lighting': True}),
             ('indirect', {'indirect': True}), ('gi-only', {'gi': True}),
             ('probe-only', {'probe': True}), ('combined', {'lighting': True, 'indirect': True}),
             ('showcase', {'scene': False}), ('fixed', {'lighting': True, 'indirect': True, 'fixed': True}),
             ('emission', {'emission_only': True}), ('half', {'alpha': .5}),
             ('node-half', {'opacity': .5}), ('combined-half', {'alpha': .5, 'opacity': .5}),
             ('depth-hidden', {'behind': True})]
    for name, options in cases:
        actual = linear(capture(name, **options))
        coverage = options.get('alpha', 1) * options.get('opacity', 1)
        color = emission if options.get('emission_only') else base + emission
        expected = receiver if options.get('behind') else color * coverage + receiver * (1 - coverage)
        error = float(np.abs(actual - expected).max())
        metrics[name + 'LinearMaximumError'] = error
        if error > .008:
            failures.append(name + ' violates unlit color or coverage')
    for isolation in ('direct-diffuse', 'ambient', 'direct-specular', 'reflection', 'hair-kk', 'rim'):
        name = 'isolation-' + isolation
        actual = capture(name, lighting=True, indirect=True, isolation=isolation)
        metrics[name + 'Maximum'] = int(actual.max())
        if actual.max() != 0:
            failures.append(name + ' contains lighting energy')
    off = capture('pbr-dark', unlit=False)
    on = capture('pbr-light', unlit=False, lighting=True)
    metrics['pbrLightingResponse'] = float(np.abs(linear(on) - linear(off)).mean())
    if metrics['pbrLightingResponse'] <= .01:
        failures.append('PBR lighting positive control is missing')
    for label, isolation, options in [('gi', 'ambient', {'gi': True}),
                                       ('probe', 'reflection', {'probe': True})]:
        off = capture('pbr-' + label + '-off', unlit=False, isolation=isolation)
        on = capture('pbr-' + label + '-on', unlit=False, isolation=isolation, **options)
        response = float(np.abs(linear(on) - linear(off)).mean())
        metrics['pbr-' + label + '-isolatedResponse'] = response
        if response <= .0005:
            failures.append('PBR ' + label + ' isolated positive control is missing')
    metrics['fixedMaximumDifference'] = int(np.abs(frames['combined'] - frames['fixed']).max())
    if metrics['fixedMaximumDifference']:
        failures.append('Fixed and indexed descriptor paths differ')
    summary = {'schemaVersion': 1, 'metrics': metrics, 'failures': failures,
        'captures': records, 'linearTolerance': .008,
        'playerSha256': hashlib.sha256(player.read_bytes()).hexdigest(),
        'legacyLitRed': legacy_lit_red,
        'inputFingerprints': frozen_inputs,
        'scope': 'Public surface output, lighting isolation, coverage, depth and resource lifetime',
        'limitations': ['Native VFX runtime binding and time-dependent sampling require separate evidence.']}
    (root / 'summary.json').write_text(json.dumps(summary, indent=2))
    assert frozen_inputs == fingerprints(), 'Capture inputs changed during validation'
    print(json.dumps({'captures': len(records), 'failures': failures, 'metrics': metrics}))
    assert not failures, failures


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--expected-validation', choices=('any', 'enabled', 'disabled'), default='any')
    parser.add_argument('--legacy-lit-red', action='store_true')
    args = parser.parse_args()
    if args.output:
        run(args.executable, args.output.resolve(), args.expected_validation, args.legacy_lit_red)
    else:
        runs = Path(__file__).resolve().parents[1] / 'build/runs'
        with tempfile.TemporaryDirectory(prefix='unlit-', dir=runs) as temporary:
            run(args.executable, Path(temporary) / 'capture', args.expected_validation, args.legacy_lit_red)
