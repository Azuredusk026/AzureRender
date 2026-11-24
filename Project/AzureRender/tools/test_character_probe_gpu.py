"""Verify character reflection controls with an isolated constant HDR probe."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile

import numpy as np
from PIL import Image

ENGINE = next(path for path in Path(__file__).resolve().parents
              if (path / 'tools/create_visibility_fixture.py').is_file())
sys.path.insert(0, str(ENGINE / 'tools'))
from create_visibility_fixture import component, surface

PROBE_RGB = np.array([8., 4., 2.])
LINEAR_BUDGET = .004
CHARACTER_BUDGET = .0005
ROI = (slice(60, 68), slice(60, 68))


def linear(image):
    encoded = image.astype(np.float64) / 255
    return np.where(encoded <= .04045, encoded / 12.92,
                    ((encoded + .055) / 1.055) ** 2.4)


def definitions():
    # F0 and material weights are the existing public surface policy. The
    # constant HDR source makes the oracle independent of projection and LOD.
    return [
        dict(name='pbr-on', material='pbr', f0=.08, weight=1, controlOnly=True),
        dict(name='pbr-off', material='pbr', f0=.08, weight=1, probe=False),
        dict(name='skin-full', material='skin', f0=.0256, weight=.05),
        dict(name='skin-half', material='skin', f0=.0256, weight=.05, strength=.5),
        dict(name='skin-zero', material='skin', f0=.0256, weight=.05, strength=0),
        dict(name='skin-off', material='skin', f0=.0256, weight=.05, probe=False),
        dict(name='face-full', material='face', f0=.0176, weight=.03),
        dict(name='hair-full', material='hair', f0=.024, weight=.08),
        dict(name='fabric-full', material='fabric', f0=.08, weight=1),
        dict(name='legacy-face', material='face', version=1, f0=.0176, weight=1),
        dict(name='face-qa-disabled', material='face', f0=.0176, weight=.03, disabled=True),
        dict(name='pbr-qa-disabled', material='pbr', f0=.08, weight=1, disabled=True),
        dict(name='skin-beauty', material='skin', f0=.0256, weight=.05, isolation='beauty'),
    ]


def expected(case):
    if not case.get('probe', True) or case.get('disabled', False):
        return np.zeros(3)
    if case.get('controlOnly'):
        return None
    strength = case.get('strength', 1) if case.get('version', 2) >= 2 else 1
    return PROBE_RGB * case['f0'] * case['weight'] * strength


def prepare(root, case):
    assets = root / 'assets'
    assets.mkdir(parents=True)
    surface(assets / 'surface.gltf')
    model = json.loads((assets / 'surface.gltf').read_text())
    material = model['materials'][0]
    material['name'] = 'ProbeControl'
    material['pbrMetallicRoughness'].update(
        baseColorFactor=[.2, .3, .4, 1], metallicFactor=0, roughnessFactor=1)
    if case['material'] != 'pbr':
        material['extras'] = {'azureRenderMaterial': {
            'schemaVersion': case.get('version', 2), 'class': case['material'],
            'features': [], 'styleParameters': [0, 0, case.get('strength', 1), 0]}}
    (assets / 'surface.gltf').write_text(json.dumps(model))
    texel = struct.pack('<4e', *PROBE_RGB, 1)
    (assets / 'probe.azenv').write_bytes(b'AZENV001' + struct.pack('<III', 2, 1, 1) + texel * 2)
    level = {'schemaVersion': 1, 'id': case['name'], 'sceneType': 'character',
        'resources': [{'id': 'surface', 'asset': 'assets:/surface.gltf'}],
        'nodes': [
            {'id': 'surface', 'resourceId': 'surface', 'components': {
                'azure.transform': component('azure.transform', {'translation': [0, 0, 0]})}},
            {'id': 'camera', 'components': {
                'azure.transform': component('azure.transform', {'translation': [0, 0, 3]}),
                'azure.camera': component('azure.camera', {'enabled': True, 'priority': 1,
                    'fieldOfView': 55, 'nearPlane': .1, 'farPlane': 20})}}],
        'lights': [], 'renderSettings': {
            'sceneLighting': True, 'skyIntensity': 0, 'cameraNear': .1, 'cameraFar': 20,
            'shadowDistance': 20, 'platform': False, 'stylized': False,
            'silhouetteOutline': False, 'innerOutline': False, 'bloomEnabled': False,
            'antiAliasing': 0, 'exposure': 0, 'toneMapping': False, 'contrast': 1,
            'saturation': 1}}
    if case.get('probe', True):
        level['reflectionProbe'] = {'version': 1, 'texture': 'assets:/probe.azenv',
            'center': [0, 0, 0], 'halfExtents': [4, 4, 4], 'capturePosition': [0, 0, 0],
            'intensity': 1, 'blendDistance': 0, 'boxProjection': False}
    (assets / 'scene.azurelevel').write_text(json.dumps(level))
    project = root / 'test.azureproject'
    project.write_text(json.dumps({'schemaVersion': 1, 'id': 'character-probe',
        'name': 'Character reflection controls', 'mounts': [{'name': 'assets', 'path': 'assets'}],
        'startupScene': 'assets:/scene.azurelevel'}))
    return project


def run(player, output, expected_validation='any'):
    output.mkdir(parents=True, exist_ok=False)
    summary = {'schemaVersion': 1, 'status': 'running', 'linearErrorBudget': LINEAR_BUDGET,
        'characterLinearErrorBudget': CHARACTER_BUDGET,
        'sourceRgb': PROBE_RGB.tolist(), 'captures': [], 'failures': [], 'metrics': {},
        'playerSha256': hashlib.sha256(player.read_bytes()).hexdigest(),
        'shaderSha256': {path.name: hashlib.sha256(path.read_bytes()).hexdigest()
                         for path in (player.parent / 'shaders').glob('*.spv')
                         if path.name in ('mesh.frag.spv', 'mesh_bindless.frag.spv')},
        'scriptSha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        'limitations': ['Public surface controls; native probe DFG, projection and mip parity require separate evidence.']}
    pixels = {}
    try:
        for backend in ('indexed', 'fixed'):
            for original in definitions():
                case = copy.deepcopy(original)
                name = backend + '-' + case['name']
                root = output / name
                project = prepare(root, case)
                print(json.dumps({'capture': name, 'status': 'starting'}), flush=True)
                isolation = case.get('isolation', 'reflection')
                command = [str(player.resolve()), '--project', str(project.resolve()),
                    '--width', '128', '--height', '128', '--capture-dir', str(root / 'frames'),
                    '--capture-frames', '1', '--fixed-frame-step', '--qa-camera', 'scene',
                    '--qa-light', 'scene', '--qa-isolation', isolation,
                    '--runtime-report', str(root / 'runtime.json')]
                if case.get('disabled'):
                    command += ['--qa-effect', 'specular', '--qa-effect-state', 'disabled']
                if backend == 'fixed':
                    command += ['--disable-bindless', '--disable-compute-skinning', '--disable-parallel-recording']
                (root / 'command.json').write_text(json.dumps(command, indent=2))
                startup = None
                if hasattr(subprocess, 'STARTUPINFO'):
                    startup = subprocess.STARTUPINFO()
                    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
                    startup.wShowWindow = 0
                result = subprocess.run(command, capture_output=True, encoding='utf8', errors='replace',
                                        timeout=120, startupinfo=startup)
                log = result.stdout + result.stderr
                (root / 'player.log').write_text(log, encoding='utf8')
                assert result.returncode == 0, (name, 'Player failed; see player.log')
                assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]', log), name
                assert f'Character texture access: {backend}' in log, (name, 'texture access path mismatch')
                assert 'Allocator after unload: buffers=0 images=0' in log, name
                validated = 'Validation layer: enabled' in log
                if expected_validation != 'any':
                    assert validated == (expected_validation == 'enabled'), (name, 'validation configuration')
                runtime = json.loads((root / 'runtime.json').read_text())
                assert not runtime['scriptErrors'] and not runtime['presentationErrors'], name
                manifest_path = root / 'frames/capture_manifest.json'
                manifest = json.loads(manifest_path.read_text())
                assert manifest['qaIsolation'] == isolation and manifest['qaCamera'] == 'scene', name
                assert manifest['qaLight'] == 'scene' and manifest['diagnosticView'] == 'beauty', name
                assert manifest['qaEffectState'] == ('disabled' if case.get('disabled') else 'enabled'), name
                assert np.allclose(manifest['qaCameraPosition'], [0, 0, 3], atol=1e-6), name
                frame = root / 'frames/frame_000000.png'
                rgb = np.asarray(Image.open(frame).convert('RGB'))
                pixels[name] = rgb
                measured = linear(rgb[ROI])
                wanted = expected(case)
                error = None if wanted is None else float(np.abs(measured - wanted).max())
                budget = (CHARACTER_BUDGET if case['material'] in ('skin', 'face', 'hair')
                          and case.get('version', 2) >= 2 else LINEAR_BUDGET)
                record = {'name': name, 'case': case, 'validationEnabled': validated,
                    'expectedLinearRgb': None if wanted is None else wanted.tolist(),
                    'meanLinearRgb': measured.mean(axis=(0, 1)).tolist(),
                    'maximumLinearError': error, 'linearErrorBudget': budget,
                    'imageSha256': hashlib.sha256(frame.read_bytes()).hexdigest(),
                    'levelSha256': hashlib.sha256((root / 'assets/scene.azurelevel').read_bytes()).hexdigest(),
                    'modelSha256': hashlib.sha256((root / 'assets/surface.gltf').read_bytes()).hexdigest(),
                    'probeSha256': hashlib.sha256((root / 'assets/probe.azenv').read_bytes()).hexdigest(),
                    'manifestSha256': hashlib.sha256(manifest_path.read_bytes()).hexdigest(),
                    'allocatorAfterUnload': {'buffers': 0, 'images': 0}}
                summary['captures'].append(record)
                if error is not None and error > budget:
                    summary['failures'].append(name + ': incorrect reflection energy')
                if wanted is not None and not np.any(wanted) and measured.max() != 0:
                    summary['failures'].append(name + ': reflection must be exactly black')
        for backend in ('indexed', 'fixed'):
            response = float(np.abs(linear(pixels[backend + '-pbr-on'][ROI])
                                    - linear(pixels[backend + '-pbr-off'][ROI])).max())
            summary['metrics'][backend + 'PbrProbeResponse'] = response
            assert response > .1, backend + ': independent PBR probe positive control is dark'
            response = float(linear(pixels[backend + '-skin-full'][ROI]).max())
            summary['metrics'][backend + 'SkinProbeResponse'] = response
            assert response > .008, backend + ': weighted skin positive control is dark'
        for case in definitions():
            error = int(np.abs(pixels['indexed-' + case['name']].astype(np.int16)
                               - pixels['fixed-' + case['name']].astype(np.int16)).max())
            summary['metrics'][case['name'] + 'BackendMaximumByteDelta'] = error
            if error > 1:
                summary['failures'].append(case['name'] + ': fixed and indexed output differ')
        summary['status'] = 'failed' if summary['failures'] else 'passed'
    except Exception as error:
        summary.update(status='failed', exception=str(error))
        raise
    finally:
        (output / 'summary.json').write_text(json.dumps(summary, indent=2))
    print(json.dumps({'status': summary['status'], 'captures': len(summary['captures']),
                      'failures': summary['failures'], 'metrics': summary['metrics']}))
    assert not summary['failures'], summary['failures']


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--expected-validation', choices=('any', 'enabled', 'disabled'), default='any')
    parser.add_argument('--prepare-only', type=Path, help='Generate public CPU fixtures without launching Player.')
    args = parser.parse_args()
    if args.prepare_only:
        for case in definitions():
            prepare(args.prepare_only / case['name'], case)
        print(json.dumps({'prepared': len(definitions()), 'gpuExecuted': False}))
    elif args.output:
        run(args.executable, args.output.resolve(), args.expected_validation)
    else:
        with tempfile.TemporaryDirectory(prefix='character-probe-', dir=ENGINE / 'build/runs') as temporary:
            run(args.executable, Path(temporary) / 'capture', args.expected_validation)
