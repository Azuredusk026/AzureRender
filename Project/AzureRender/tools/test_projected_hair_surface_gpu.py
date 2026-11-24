"""Verify independently selected strand/diffuse surface energy through Player.

The ramp suite retains its original oracle. This suite isolates the surface
response, scene point/directional lights, generic specular exclusion, and RS
beauty energy. Native runtime light globals and P.B shadow consumers remain
outside this contract.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile

import numpy as np
from PIL import Image

import test_projected_hair_gpu as ramp


def definitions():
    return [
        ('strand-directional', 'direct-diffuse', 'strand-diffuse', False, False, False),
        ('layered-directional', 'direct-diffuse', 'layered', False, False, False),
        ('strand-point', 'direct-diffuse', 'strand-diffuse', True, False, False),
        ('strand-dark', 'beauty', 'strand-diffuse', False, True, False),
        ('strand-direct-specular', 'direct-specular', 'strand-diffuse', False, False, False),
        ('layered-direct-specular', 'direct-specular', 'layered', False, False, False),
        ('strand-reflection', 'reflection', 'strand-diffuse', False, True, True),
        ('layered-reflection', 'reflection', 'layered', False, True, True),
        ('strand-rs-beauty', 'beauty', 'strand-diffuse', False, False, False),
        ('strand-zero-diffuse', 'direct-diffuse', 'strand-diffuse', False, False, False),
    ]


def expected_diffuse(case, *, scale):
    result = []
    extent = np.tan(np.deg2rad(ramp.FOV) * .5) * ramp.CAMERA[2]
    for _, x, y in ramp.samples(case):
        point = np.array([((x + .5) / ramp.SIZE * 2 - 1) * extent,
                          (1 - (y + .5) / ramp.SIZE * 2) * extent, 0])
        intensity = ramp.LIGHT_INTENSITY
        if case['local']:
            delta = np.array([0, .16, 4.]) - point
            distance = np.linalg.norm(delta)
            normal_dot_light = delta[2] / distance
            intensity *= max(1 - distance / 10, 0)**2 / (1 + distance**2 * .12)
        else:
            normal_dot_light = np.sqrt(1 - .04**2)
        result.append(ramp.linear(ramp.CONTROL[:3]) * scale * intensity * normal_dot_light)
    return np.asarray(result)


def run(player, output, *, require_validation=False):
    output.mkdir(parents=True, exist_ok=False)
    summary = {'schemaVersion': 1, 'status': 'running', 'linearErrorBudget': ramp.BUDGET,
        'playerSha256': hashlib.sha256(player.read_bytes()).hexdigest(), 'captures': [],
        'limitations': ['Validates explicit host surface response and actual scene-light input.',
            'Native P.B shadow/ramp/screen-rim consumers and runtime globals remain unverified.']}
    measured_cases = {}
    try:
        for name, isolation, mode, local, dark, environment in definitions():
            print(json.dumps({'capture': name, 'status': 'starting'}), flush=True)
            root = output / name
            case = ramp.cases()[0]
            case.update(name=name, local=local, dark=dark)
            project, _ = ramp.prepare(case, root)
            asset_path = root / 'assets/panels.gltf'
            asset = json.loads(asset_path.read_text())
            rs_only = name == 'strand-rs-beauty'
            scale = 0 if name == 'strand-zero-diffuse' else (.96 if mode == 'strand-diffuse' else 1)
            for material in asset['materials']:
                material['extras']['azureRenderMaterial']['styleParameters'] = [0, 0, 1, 0]
                profile = material['extras']['azureRenderMaterial']['hair']['projectedRamp']
                profile.update(surfaceResponse=mode, diffuseScale=scale)
                if not rs_only:
                    profile.update(intensity=0, secondaryColor=[0, 0, 0])
            if rs_only:
                asset['images'][0]['uri'] = ramp.image_uri(Image.new('RGBA', (2, 2), (0, 0, 0, 255)))
            asset_path.write_text(json.dumps(asset), encoding='utf8')
            level_path = root / 'assets/scene.azurelevel'
            level = json.loads(level_path.read_text())
            level['renderSettings']['skyIntensity'] = 4 if environment else 0
            if isolation == 'direct-specular': level['lights'][0]['intensity'] = 16
            level_path.write_text(json.dumps(level), encoding='utf8')
            command = [str(player.resolve()), '--project', str(project.resolve()),
                '--width', str(ramp.SIZE), '--height', str(ramp.SIZE),
                '--capture-dir', str((root / 'frames').resolve()), '--capture-frames', '1',
                '--fixed-frame-step', '--qa-camera', 'scene', '--qa-light', 'scene',
                '--qa-isolation', isolation, '--runtime-report', str((root / 'runtime.json').resolve())]
            (root / 'command.json').write_text(json.dumps(command, indent=2), encoding='utf8')
            with (root / 'player.log').open('w', encoding='utf8') as stream:
                subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT, timeout=120, check=True)
            log = (root / 'player.log').read_text(encoding='utf8', errors='replace')
            assert not re.search(r'VUID-|Validation Error|Validation Warning|\[error\]', log), name
            assert 'Allocator after unload: buffers=0 images=0' in log, name
            validation = 'Validation layer: enabled' in log
            assert validation or not require_validation, name + ': validation required'
            runtime = json.loads((root / 'runtime.json').read_text())
            assert not runtime['scriptErrors'] and not runtime['presentationErrors'], name
            frame = root / 'frames/frame_000000.png'
            pixels = np.asarray(Image.open(frame).convert('RGB'))
            measured = np.asarray([ramp.linear(pixels[y, x]) for _, x, y in ramp.samples(case)])
            measured_cases[name] = measured
            if isolation == 'direct-diffuse':
                wanted = expected_diffuse(case, scale=scale * (.62 if mode == 'layered' else 1))
            elif rs_only:
                wanted = np.asarray([row['linearRgb'] for row in ramp.expectation(case)]) / 3
            elif mode == 'strand-diffuse':
                wanted = np.zeros_like(measured)
            else:
                wanted = None
                assert float(measured.max()) > .001, name + ': positive layered control is dark'
            maximum = None if wanted is None else float(np.abs(measured - wanted).max())
            if wanted is not None:
                (root / 'expected.json').write_text(json.dumps({'linearRgb': wanted.tolist(),
                    'pixels': [[x, y] for _, x, y in ramp.samples(case)]}, indent=2), encoding='utf8')
                assert maximum <= ramp.BUDGET, f'{name}: linear error {maximum} exceeds {ramp.BUDGET}'
                if not np.any(wanted): assert float(measured.max()) == 0, name + ': extra surface energy'
            summary['captures'].append({'name': name, 'validationEnabled': validation,
                'maximumLinearError': maximum, 'maximumLinearValue': float(measured.max()),
                'imageSha256': hashlib.sha256(frame.read_bytes()).hexdigest(),
                'allocatorAfterUnload': {'buffers': 0, 'images': 0}})
        assert float(measured_cases['strand-rs-beauty'].max()) > .01
        summary['status'] = 'passed'
    except Exception as error:
        summary.update(status='failed', failure=str(error))
        raise
    finally:
        (output / 'summary.json').write_text(json.dumps(summary, indent=2), encoding='utf8')
    print(json.dumps({'status': summary['status'], 'caseCount': len(summary['captures'])}))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--require-validation', action='store_true')
    args = parser.parse_args()
    if args.output:
        run(args.executable, args.output.resolve(), require_validation=args.require_validation)
    else:
        runs = Path(__file__).resolve().parents[1] / 'build/runs'
        runs.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='projected-hair-surface-', dir=runs) as temporary:
            run(args.executable, Path(temporary) / 'capture', require_validation=args.require_validation)
