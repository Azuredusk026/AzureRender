"""Measure public lens offsets in raster and path-traced production captures."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile
import numpy as np
from PIL import Image
from native_render.light_lab import generate


def run(player, output, expected_validation):
    output.mkdir(parents=True, exist_ok=False)
    report = {'schemaVersion': 1, 'captures': [], 'status': 'running'}
    images = {}
    try:
        cases = [(renderer, ortho, variant) for renderer in ('character', 'pathtracing')
                 for ortho in (False, True) for variant in ('base', 'xy')]
        cases += [('character', True, v) for v in ('fixed', 'legacy')]
        cases += [(renderer, False, v) for renderer in ('character', 'sample') for v in ('sky-base', 'sky-xy')]
        for renderer, ortho, variant in cases:
            name = f'{renderer}-{"ortho" if ortho else "perspective"}-{variant}'
            print(name, flush=True)
            folder = output/name
            generate(folder, 'dark')
            model_path = folder/'plane.gltf'
            model = json.loads(model_path.read_text())
            model['materials'][0].update(emissiveFactor=[.1, .02, .01])
            model['materials'][0]['pbrMetallicRoughness']['baseColorFactor'] = [0, 0, 0, 1]
            model_path.write_text(json.dumps(model), encoding='utf-8')
            scene_path = folder/'level.azurelevel'
            scene = json.loads(scene_path.read_text())
            scene['nodes'][0]['components'] = {'azure.transform': {
                'type': 'azure.transform', 'version': 1, 'data': {'scale': [.15]*3}}}
            camera = scene['nodes'][1]['components']['azure.camera']
            offset = [.5, .25] if variant in ('xy', 'fixed', 'sky-xy') else [0, 0]
            camera.update(version=1 if variant == 'legacy' else 2)
            camera['data'].update(orthographic=ortho, fieldOfView=60)
            if variant != 'legacy':
                camera['data'].update(projectionOffsetX=offset[0], projectionOffsetY=offset[1])
            scene['renderSettings'].update(renderer=renderer, pathTraceSamples=8,
                pathTraceBounces=1, pathTraceDenoise=False, skyMode=2, skyColor=[0, 0, 0])
            if variant.startswith('sky-'):
                scene['renderSettings'].update(skyMode=0, skyIntensity=1, background=True)
                if renderer == 'sample':
                    scene['resources'] = []; scene['nodes'] = [scene['nodes'][1]]
            scene_path.write_text(json.dumps(scene), encoding='utf-8')
            command = [str(player.resolve()), '--project', str(folder/'lab.azureproject'),
                '--width', '128', '--height', '128', '--capture-dir', str(folder/'frames'),
                '--capture-frames', '1', '--fixed-frame-step', '--qa-camera', 'scene', '--qa-light', 'scene']
            if variant == 'fixed':
                command += ['--disable-bindless', '--disable-compute-skinning', '--disable-parallel-recording']
            (folder/'command.json').write_text(json.dumps(command, indent=2), encoding='utf-8')
            with (folder/'player.log').open('w', encoding='utf-8') as log:
                subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=120)
            log = (folder/'player.log').read_text(encoding='utf-8', errors='replace')
            assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]', log), name
            assert 'Allocator after unload: buffers=0 images=0' in log, name
            assert ('Validation layer: enabled' in log) == (expected_validation == 'enabled'), name
            manifest = json.loads((folder/'frames/capture_manifest.json').read_text())
            assert manifest['cameraProjection'] == {'schemaVersion': 2, 'offsetNdc': offset}, manifest['cameraProjection']
            if renderer == 'pathtracing':
                assert manifest['pathTracingCameraLayoutVersion'] == 2
            frame = folder/'frames/frame_000000.png'
            pixels = np.asarray(Image.open(frame).convert('RGB')); images[name] = pixels
            mask = (pixels[:, :, 0] > 20) & (pixels[:, :, 0].astype(float) > pixels[:, :, 1]*1.8)
            ys, xs = np.where(mask)
            center = None
            if not variant.startswith('sky-'):
                assert len(xs) > 30, (name, len(xs))
                center = [(float(xs.min())+float(xs.max()))/2, (float(ys.min())+float(ys.max()))/2]
            report['captures'].append({'name': name, 'center': center, 'offsetNdc': offset,
                'imageSha256': hashlib.sha256(frame.read_bytes()).hexdigest()})
        records = {r['name']: r for r in report['captures']}
        for renderer in ('character', 'pathtracing'):
            for projection in ('ortho', 'perspective'):
                prefix = f'{renderer}-{projection}-'
                shift = np.asarray(records[prefix+'xy']['center'])-records[prefix+'base']['center']
                assert np.max(np.abs(shift-[32, 16])) <= .75, (prefix, shift)
        for a, b in (('character-ortho-xy', 'character-ortho-fixed'),
                     ('character-ortho-base', 'character-ortho-legacy')):
            assert np.array_equal(images[a], images[b]), (a, b)
        for renderer in ('character', 'sample'):
            prefix = f'{renderer}-perspective-sky-'
            base = images[prefix+'base'][:48, :96].astype(int)
            shifted = images[prefix+'xy'][16:64, 32:128].astype(int)
            assert base.max()-base.min() > 15, renderer
            assert np.abs(base-shifted).max() <= 1, (renderer, np.abs(base-shifted).max())
        report['status'] = 'passed'
    except Exception as error:
        report.update(status='failed', error=str(error)); raise
    finally:
        (output/'summary.json').write_text(json.dumps(report, indent=2), encoding='utf-8')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--expected-validation', choices=('enabled', 'disabled'), required=True)
    args = parser.parse_args()
    if args.output:
        run(args.executable, args.output.resolve(), args.expected_validation)
    else:
        with tempfile.TemporaryDirectory(prefix='camera-projection-', dir=Path(__file__).resolve().parents[1]/'build/runs') as temporary:
            run(args.executable, Path(temporary)/'capture', args.expected_validation)
