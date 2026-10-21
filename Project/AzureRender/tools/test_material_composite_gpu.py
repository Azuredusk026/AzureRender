"""Verify linear multiply overlays through the production Player and glTF path."""
import argparse
import base64
import copy
import hashlib
import json
from pathlib import Path
import re
import subprocess
import struct
import tempfile

import numpy as np
from PIL import Image
from create_visibility_fixture import surface, component


def linear(image):
    value = image.astype(np.float64) / 255
    return np.where(value <= .04045, value / 12.92, ((value + .055) / 1.055) ** 2.4)


def run(player, root, *, legacy_alpha_red=False, expected_validation='any'):
    root.mkdir(parents=True, exist_ok=False)
    assets = root / 'assets'
    assets.mkdir()
    surface(assets / 'rear.gltf', double_sided=True)
    rear = json.loads((assets / 'rear.gltf').read_text())
    rear['materials'][0]['pbrMetallicRoughness']['baseColorFactor'] = [.64, .36, .16, 1]
    (assets / 'rear.gltf').write_text(json.dumps(rear))
    surface(assets / 'overlay.gltf', sloped=True)
    template = json.loads((assets / 'overlay.gltf').read_text())
    Image.new('RGBA', (1, 1), (255, 255, 255, 128)).save(assets / 'alpha.png')
    # Constant public-format fixtures cover both indirect-light additions.
    cell = struct.pack('<16f', 0, 0, 0, .12, 0, 0, 0, .08,
                       0, 0, 0, .04, 1, 0, 0, 0)
    (assets / 'field.aziv').write_bytes(
        struct.pack('<8s4f3I', b'AZIV0001', -4, -4, -4, 8, 2, 2, 2) + cell * 8)
    (assets / 'probe.azenv').write_bytes(
        b'AZENV001' + struct.pack('<III', 2, 1, 1)
        + struct.pack('<8e', .1, .18, .3, 1, .1, .18, .3, 1))
    project = root / 'test.azureproject'
    project.write_text(json.dumps({'schemaVersion': 1, 'id': 'material-composite',
        'name': 'Multiply overlay contract', 'mounts': [{'name': 'assets', 'path': 'assets'}],
        'startupScene': 'assets:/scene.azurelevel'}))
    records = []
    frames = {}
    tint = np.array([.25, .5, .75])
    metrics = {}
    failures = []
    limitations = [
        'PNG RGB checks do not read destination alpha or raw normal RGBA.',
        'Selection, shadow casting, mixed transparent ordering and resize are not GPU-verified by this script.',
        'Beauty enables GI and a reflection probe together; their separate contributions are not isolated.',
    ]

    def finish(**extra):
        summary = {'metrics': metrics, 'failures': failures, 'captures': records,
            'limitations': limitations, 'legacyAlphaRed': legacy_alpha_red,
            'expectedValidation': expected_validation,
            'lightingAssetSha256': {name: hashlib.sha256((assets / name).read_bytes()).hexdigest()
                                   for name in ('field.aziv', 'probe.azenv')},
            'playerSha256': hashlib.sha256(player.read_bytes()).hexdigest(), **extra}
        (root / 'summary.json').write_text(json.dumps(summary, indent=2))
        print(json.dumps({'metrics': metrics, 'failures': failures,
            'captures': len(records), 'validationEnabled': all(row['validationEnabled'] for row in records), **extra}))

    def capture(name, alpha=1, *, visible=True, multiply=True, opacity=1,
                mirror=False, reverse=False, double=False, behind=False,
                fixed=False, isolation='albedo', material_class='overlay',
                scene_lighting=True, texture_alpha=False, opaque=False,
                lighting=False, light_enabled=True, coplanar=False):
        assert not (opaque and multiply), 'opaque control must use the ordinary material path'
        assert not (coplanar and behind), 'a card cannot be both coplanar and behind'
        model = copy.deepcopy(rear if coplanar else template)
        material = model['materials'][0]
        material.update(alphaMode='OPAQUE' if opaque else 'BLEND', doubleSided=double)
        material['pbrMetallicRoughness']['baseColorFactor'] = [*tint, alpha]
        profile = {'schemaVersion': 2, 'class': material_class,
                   'features': ['overlay'] if material_class == 'overlay' else []}
        if multiply:
            profile['features'].append('multiply-composite')
            profile['composite'] = {'schemaVersion': 1, 'mode': 'multiply'}
        material['extras'] = {'azureRenderMaterial': profile}
        if texture_alpha:
            # A white texel isolates alpha; valid UVs keep the glTF self-contained.
            uv = struct.pack('<8f', 0, 0, 1, 0, 1, 1, 0, 1)
            buffer_index, view_index, accessor_index = (
                len(model['buffers']), len(model['bufferViews']), len(model['accessors']))
            model['buffers'].append({'byteLength': len(uv),
                'uri': 'data:application/octet-stream;base64,' + base64.b64encode(uv).decode()})
            model['bufferViews'].append({'buffer': buffer_index, 'byteLength': len(uv)})
            model['accessors'].append({'bufferView': view_index, 'componentType': 5126,
                                      'count': 4, 'type': 'VEC2'})
            model['meshes'][0]['primitives'][0]['attributes']['TEXCOORD_0'] = accessor_index
            model['images'] = [{'uri': 'alpha.png'}]
            model['textures'] = [{'source': 0}]
            material['pbrMetallicRoughness']['baseColorTexture'] = {'index': 0}
        model['extensionsUsed'] = ['AZURE_node_opacity']
        model['nodes'][0]['extensions'] = {'AZURE_node_opacity': {'schemaVersion': 1, 'opacity': opacity}}
        (assets / 'overlay.gltf').write_text(json.dumps(model))
        nodes = [
            {'id': 'rear', 'resourceId': 'rear', 'components': {
                'azure.transform': component('azure.transform', {'translation': [0, 0, -1], 'scale': [4, 4, 4]})}},
            {'id': 'overlay', 'resourceId': 'overlay', 'components': {
                'azure.transform': component('azure.transform', {
                    'translation': [0, 0, -1 if coplanar else (-2 if behind else 0)],
                    'rotation': [0, 180 if reverse else 0, 0],
                    'scale': [-1 if mirror else 1, 1, 1]}),
                'azure.renderable': component('azure.renderable', {'visible': visible})}},
            {'id': 'camera', 'components': {
                'azure.transform': component('azure.transform', {'translation': [0, 0, 3]}),
                'azure.camera': component('azure.camera', {'enabled': True, 'priority': 1,
                    'fieldOfView': 55, 'nearPlane': .1, 'farPlane': 20})}}]
        level = {'schemaVersion': 1, 'id': name, 'sceneType': 'character', 'lights': [],
            'resources': [{'id': 'rear', 'asset': 'assets:/rear.gltf'},
                          {'id': 'overlay', 'asset': 'assets:/overlay.gltf'}],
            'nodes': nodes, 'renderSettings': {'sceneLighting': scene_lighting, 'skyIntensity': 0,
                'cameraNear': .1, 'cameraFar': 20, 'shadowDistance': 20,
                'platform': False, 'silhouetteOutline': False, 'innerOutline': False,
                'stylized': False, 'bloomEnabled': False, 'antiAliasing': 0,
                'exposure': 0, 'toneMapping': False, 'contrast': 1, 'saturation': 1,
                'diagnosticView': {'world-normal': 1, 'depth': 4}.get(isolation, 0)}}
        if lighting:
            nodes.append({'id': 'light', 'components': {
                'azure.transform': component('azure.transform', {'translation': [0, 0, 2]})}})
            level['lights'] = [{'id': 'light', 'nodeId': 'light', 'color': [1, 1, 1],
                                'intensity': 4, 'radius': 10, 'enabled': light_enabled}]
            level['irradianceVolume'] = {'version': 1, 'asset': 'assets:/field.aziv',
                                         'enabled': True, 'intensity': 1}
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
        assert result.returncode == 0 and not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]', log), name + '\n' + log[-3000:]
        validation_enabled='Validation layer: enabled' in log
        if expected_validation!='any':
            assert validation_enabled==(expected_validation=='enabled'), (name, 'validation layer configuration')
        assert 'Allocator after unload: buffers=0 images=0' in log, name
        report = json.loads((output / 'runtime.json').read_text())
        assert not report['scriptErrors'] and not report['presentationErrors'], name
        manifest_path = output / 'frames/capture_manifest.json'
        manifest = json.loads(manifest_path.read_text())
        expected_view = {'world-normal': 'world-normal', 'depth': 'depth'}.get(isolation, 'beauty')
        assert manifest['qaIsolation'] == isolation, (name, 'isolation overridden', manifest['qaIsolation'])
        assert manifest['diagnosticView'] == expected_view, (name, 'diagnostic view overridden', manifest['diagnosticView'])
        assert manifest['qaCamera'] == 'scene', (name, 'scene camera overridden')
        assert np.allclose(manifest['qaCameraPosition'], [0, 0, 3], atol=1e-6), name
        assert np.allclose(manifest['qaCameraTarget'], [0, 0, 2], atol=1e-6), name
        assert abs(manifest['cameraNear'] - .1) < 1e-6 and abs(manifest['cameraFar'] - 20) < 1e-6, name
        frame = output / 'frames/frame_000000.png'
        frames[name] = np.asarray(Image.open(frame).convert('RGB'), dtype=np.int16)
        records.append({'name': name, 'imageSha256': hashlib.sha256(frame.read_bytes()).hexdigest(),
            'materialSha256': hashlib.sha256((assets / 'overlay.gltf').read_bytes()).hexdigest(),
            'levelSha256': hashlib.sha256((assets / 'scene.azurelevel').read_bytes()).hexdigest(),
            'manifestSha256': hashlib.sha256(manifest_path.read_bytes()).hexdigest(),
            'qaIsolation': manifest['qaIsolation'], 'diagnosticView': manifest['diagnosticView'],
            'materialClass': material_class, 'sceneLighting': scene_lighting,
            'multiply': multiply, 'visible': visible,
            'lighting': lighting, 'lightEnabled': lighting and light_enabled,
            'coplanar': coplanar,
            'effectiveAlpha': alpha * opacity * (128 / 255 if texture_alpha else 1),
            'validationEnabled': validation_enabled})
        return frames[name][54:74, 54:74]

    # The receiver captures the real transfer function and its quantization.
    baseline = capture('hidden', visible=False, multiply=False)
    destination = linear(baseline)
    baseline_error = float(np.abs(destination - np.array([.64, .36, .16])).max())
    metrics['receiverLinearMaximumError'] = baseline_error
    assert baseline_error < .008, 'receiver baseline is absent or its transfer function changed'
    if legacy_alpha_red:
        # This mode uses only the existing ordinary-alpha profile. Its red
        # result must come from the formula, never from a missing feature bit.
        pixels = capture('legacy-alpha-red', .5, multiply=False)
        error = float(np.abs(linear(pixels) - destination * (.5 + tint * .5)).max())
        metrics['legacyAlphaMultiplyLinearMaximumError'] = error
        red_observed = error > .008
        failures.append('ordinary alpha violates multiply composition' if red_observed
                        else 'legacy red fixture did not distinguish the blend formulas')
        finish(expectedRedObserved=red_observed)
        raise AssertionError(failures)
    captures = {
        'zero': (capture('zero', 0), 0),
        'half': (capture('half', .5), .5),
        'full': (capture('full'), 1),
        'node-zero': (capture('node-zero', opacity=0), 0),
        'node-half': (capture('node-half', opacity=.5), .5),
        'combined-alpha': (capture('combined-alpha', .5, opacity=.5, texture_alpha=True), .25 * 128 / 255),
        'coplanar': (capture('coplanar', coplanar=True), 1),
        'mirror': (capture('mirror', mirror=True), 1),
        'double-back': (capture('double-back', reverse=True, double=True), 1)}
    for name, (pixels, alpha) in captures.items():
        expected = destination * (1 - alpha + tint * alpha)
        error = float(np.abs(linear(pixels) - expected).max())
        metrics[name + 'LinearMaximumError'] = error
        if error > .008:
            failures.append(name + ' violates linear multiply composition')
    alpha_pixels = capture('alpha-over', .5, multiply=False)
    error = float(np.abs(linear(alpha_pixels) - (destination * .5 + tint * .5)).max())
    metrics['alphaOverLinearMaximumError'] = error
    if error > .008:
        failures.append('ordinary alpha blending changed')
    for name, options in [('depth-hidden', {'behind': True}), ('culled-back', {'reverse': True})]:
        pixels = capture(name, **options)
        error = int(np.abs(pixels - baseline).max())
        metrics[name + 'MaximumDifference'] = error
        if error > 1:
            failures.append(name + ' must preserve the receiver')
    fixed = capture('fixed', fixed=True)
    error = int(np.abs(fixed - captures['full'][0]).max())
    metrics['fallbackMaximumDifference'] = error
    if error > 1:
        failures.append('fixed descriptors and CPU skinning must match')
    beauty_hidden = capture('beauty-hidden', visible=False, multiply=False,
                            isolation='beauty', lighting=True)
    beauty_light_off = capture('beauty-light-off', visible=False, multiply=False,
                               isolation='beauty', lighting=True, light_enabled=False)
    beauty_destination = linear(beauty_hidden)
    response = float(np.abs(beauty_destination - linear(beauty_light_off)).max())
    metrics['beautyDirectLightResponse'] = response
    metrics['beautyReceiverLinearRange'] = [float(beauty_destination.min()), float(beauty_destination.max())]
    assert response > .005, 'Beauty receiver must respond to the actual scene light'
    assert beauty_destination.min() > .005 and beauty_destination.max() < .95, 'Beauty receiver is dark or clipped'
    beauty_full = capture('beauty-full', isolation='beauty', lighting=True)
    error = float(np.abs(linear(beauty_full) - beauty_destination * tint).max())
    metrics['beautyLinearMaximumError'] = error
    if error > .008:
        failures.append('Beauty multiply must use the unlit tint after GI and reflection evaluation')
    # Generic materials outside scene lighting have nonzero normal participation.
    # An opaque sloped control proves each diagnostic can distinguish the card.
    for isolation, prefix in [('world-normal', 'normals'), ('depth', 'depth-attachment')]:
        options = {'isolation': isolation, 'material_class': 'generic', 'scene_lighting': False}
        hidden = capture(prefix + '-hidden', visible=False, multiply=False, **options)
        control = capture(prefix + '-opaque-control', multiply=False, opaque=True, **options)
        control_response = int(np.abs(hidden - control).max())
        metrics[prefix + 'OpaqueControlResponse'] = control_response
        assert control_response > 8, prefix + ' fixture cannot distinguish the nearer sloped card'
        if isolation == 'world-normal':
            error = float(np.abs(linear(hidden) - np.array([.5, .5, 1])).max())
            metrics['normalBaselineLinearMaximumError'] = error
            assert error < .008, 'receiver normal baseline is hidden or invalid'
        else:
            # The plane lies four metres from a .1-to-20 metre perspective lens.
            depth = 20 / 19.9 - (20 * .1 / 19.9) / 4
            expected = 1 - depth ** 32
            error = float(np.abs(linear(hidden) - expected).max())
            metrics['depthBaselineLinearMaximumError'] = error
            assert error < .008, 'receiver depth baseline is hidden or invalid'
        overlay = capture(prefix + '-overlay', **options)
        error = int(np.abs(hidden - overlay).max())
        metrics[prefix + 'MaximumDifference'] = error
        if error > 1:
            failures.append(prefix + ' multiply must preserve the receiver attachment')
    finish()
    assert not failures, failures


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', required=True, type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--legacy-alpha-red', action='store_true',
        help='Save an expected formula failure using only ordinary alpha blending; exits nonzero.')
    parser.add_argument('--expected-validation', choices=('any','enabled','disabled'), default='any')
    args = parser.parse_args()
    if args.output:
        run(args.executable, args.output.resolve(), legacy_alpha_red=args.legacy_alpha_red,
            expected_validation=args.expected_validation)
    else:
        runs = Path(__file__).resolve().parents[1] / 'build/runs'
        runs.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='material-composite-', dir=runs) as temporary:
            run(args.executable, Path(temporary) / 'capture', legacy_alpha_red=args.legacy_alpha_red,
                expected_validation=args.expected_validation)
