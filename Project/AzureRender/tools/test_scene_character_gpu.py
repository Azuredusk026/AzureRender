"""Exercise character lighting through the real scene-light Player path."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile

import numpy as np
from PIL import Image
from create_visibility_fixture import surface, component


def run(player, root, reference=None):
    root.mkdir(parents=True, exist_ok=False)
    assets = root / 'assets'
    assets.mkdir()
    surface(assets / 'surface.gltf', double_sided=True)
    baseline = json.loads((assets / 'surface.gltf').read_text())
    baseline['materials'][0]['pbrMetallicRoughness'].update(
        baseColorFactor=[.25, .12, .06, 1], metallicFactor=0, roughnessFactor=.6)
    frames = {}
    records = []

    def capture(name, *, hair=True, enabled=True, color=(1, 1, 1), fixed=False,
                isolation='hair-kk', effect=False):
        model = copy.deepcopy(baseline)
        model['materials'][0]['extras'] = {'azureRenderMaterial': {
            'schemaVersion': 2, 'class': 'hair' if hair else 'generic',
            'features': ['hair-anisotropy'] if hair else [],
            'hair': {'power': 200, 'strength': 1, 'rampRow': 3,
                     'strandShift': .1} if hair else {}}}
        if not hair:
            model['materials'][0]['extras']['azureRenderMaterial'].pop('hair')
        (assets / 'surface.gltf').write_text(json.dumps(model))
        level = {'schemaVersion': 1, 'id': name, 'sceneType': 'character',
            'renderSettings': {'platform': False, 'silhouetteOutline': False,
                'innerOutline': False, 'stylized': False, 'sceneLighting': True,
                'skyIntensity': 0, 'bloomEnabled': False, 'antiAliasing': 0,
                'exposure': 0, 'toneMapping': False, 'contrast': 1, 'saturation': 1},
            'resources': [{'id': 'surface', 'asset': 'assets:/surface.gltf'}],
            'nodes': [{'id': 'surface', 'resourceId': 'surface'},
                {'id': 'camera', 'components': {
                    'azure.transform': component('azure.transform', {'translation': [0, 0, 3]}),
                    'azure.camera': component('azure.camera', {'enabled': True, 'priority': 1, 'fieldOfView': 55})}},
                {'id': 'light', 'components': {
                    'azure.transform': component('azure.transform', {'translation': [0, 0, 2]})}}],
            'lights': [{'id': 'light', 'nodeId': 'light', 'color': color,
                        'intensity': 4, 'radius': 10, 'enabled': enabled}]}
        (assets / 'scene.azurelevel').write_text(json.dumps(level))
        project = root / 'test.azureproject'
        project.write_text(json.dumps({'schemaVersion': 1, 'id': 'scene-character',
            'name': 'Scene character fixture', 'mounts': [{'name': 'assets', 'path': 'assets'}],
            'startupScene': 'assets:/scene.azurelevel'}))
        output = root / name
        output.mkdir()
        command = [str(player.resolve()), '--project', str(project.resolve()), '--width', '128',
            '--height', '128', '--capture-dir', str((output / 'frames').resolve()),
            '--capture-frames', '1', '--fixed-frame-step', '--qa-camera', 'scene',
            '--qa-light', 'scene', '--qa-isolation', isolation,
            '--runtime-report', str((output / 'runtime.json').resolve())]
        if fixed:
            command += ['--disable-bindless', '--disable-compute-skinning', '--disable-parallel-recording']
        if effect:
            command += ['--qa-effect', 'hair-kk', '--qa-effect-state', 'disabled']
        (output / 'command.json').write_text(json.dumps(command, indent=2))
        with (output / 'player.log').open('w', encoding='utf8') as stream:
            subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT, check=True, timeout=120)
        log = (output / 'player.log').read_text(encoding='utf8', errors='replace')
        assert not re.search(r'VUID-|Validation Error|\[error\]', log), name
        assert 'Allocator after unload: buffers=0 images=0' in log, name
        runtime = json.loads((output / 'runtime.json').read_text())
        assert not runtime['scriptErrors'] and not runtime['presentationErrors'], name
        frame = output / 'frames/frame_000000.png'
        frames[name] = np.asarray(Image.open(frame).convert('RGB'), dtype=np.int16)
        records.append({'name': name, 'image': str(frame.resolve()),
            'sha256': hashlib.sha256(frame.read_bytes()).hexdigest(),
            'validationEnabled': 'Validation layer: enabled' in log})
        return frames[name][50:78, 50:78]

    lit = capture('hair-lit')
    dark = capture('hair-dark', enabled=False)
    off = capture('hair-disabled', effect=True)
    green = capture('hair-green', color=(0, 1, 0))
    fixed = capture('hair-fixed', fixed=True)
    capture('pbr-lit', hair=False, isolation='beauty')
    capture('pbr-dark', hair=False, enabled=False, isolation='beauty')
    metrics = {'hairLitMean': float(lit.mean()), 'hairDarkMaximum': int(dark.max()),
        'hairDisabledMaximum': int(off.max()), 'hairEffectDifference': float(np.abs(lit-off).mean()),
        'hairGreenChannels': green.mean(axis=(0, 1)).tolist(),
        'fixedMaximumDifference': int(np.abs(lit-fixed).max())}
    failures = []
    if metrics['hairLitMean'] <= 2: failures.append('scene light must produce a visible hair KK contribution')
    if metrics['hairDarkMaximum'] != 0: failures.append('unlit hair must not manufacture direct highlights')
    if metrics['hairDisabledMaximum'] != 0: failures.append('hair effect disable must remove the highlight')
    if metrics['hairEffectDifference'] <= 2: failures.append('scene hair effect toggle must affect pixels')
    if not (green[..., 1].mean() > 2 and green[..., (0, 2)].max() == 0):
        failures.append('KK must consume the actual scene light color')
    if metrics['fixedMaximumDifference'] != 0: failures.append('descriptor variants must match')
    if reference:
        for name in ('pbr-lit', 'pbr-dark'):
            previous = np.asarray(Image.open(reference / name / 'frames/frame_000000.png').convert('RGB'), dtype=np.int16)
            difference = int(np.abs(frames[name]-previous).max())
            metrics[name + 'ReferenceMaximumDifference'] = difference
            if difference != 0: failures.append(name + ' changed from the original PBR path')
    report = {'metrics': metrics, 'failures': failures, 'captures': records,
        'player': str(player.resolve()), 'playerSha256': hashlib.sha256(player.read_bytes()).hexdigest()}
    (root / 'summary.json').write_text(json.dumps(report, indent=2))
    print(json.dumps(report['metrics']))
    assert not failures, failures


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', required=True, type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--reference', type=Path)
    args = parser.parse_args()
    if args.output:
        run(args.executable, args.output.resolve(), args.reference)
    else:
        runs = Path(__file__).resolve().parents[1] / 'build/runs'
        runs.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='scene-character-', dir=runs) as temporary:
            run(args.executable, Path(temporary) / 'capture', args.reference)
