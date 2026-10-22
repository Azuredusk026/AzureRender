"""Compare document overrides and glTF emission through both descriptor paths."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import uuid

import numpy as np
from PIL import Image
from test_character_probe_gpu import prepare, linear, ROI


def run(player, output):
    output.mkdir(parents=True, exist_ok=False)
    results, images = [], {}
    for backend in ('indexed', 'fixed'):
        for mode in ('source', 'override', 'replace', 'zero'):
            key = f'{backend}-{mode}'
            root = output / key
            project = prepare(root, dict(name=key, material='fabric', probe=False))
            model_path = root / 'assets/surface.gltf'
            model = json.loads(model_path.read_text())
            material = model['materials'][0]
            Image.new('RGBA', (1, 1), (100, 140, 180, 77)).save(root / 'assets/emission.png')
            model['images'] = [{'uri': 'emission.png'}]
            model['textures'] = [{'source': 0}]
            material['emissiveTexture'] = {'index': 0}
            material['emissiveFactor'] = [.5, .5, .5] if mode == 'source' else [1, 1, 1]
            material['extras']['azureSlotId'] = 'surface'
            material['extras']['azureRenderMaterial']['specularInput'] = {
                'schemaVersion': 1, 'source': 'emissive-alpha', 'scale': .5}
            model_path.write_text(json.dumps(model))
            if mode != 'source':
                identity = str(uuid.uuid4())
                document = root / 'assets/override.azurematerial'
                content = {'schemaVersion': 2, 'mode': 'parameters',
                    'typeId': 'azure.pbr', 'parameters': {'emissiveStrength': .5 if mode == 'override' else 0},
                    'textures': {}}
                if mode == 'replace':
                    image_id = str(uuid.uuid4())
                    (root / 'assets/emission.png.azmeta').write_text(json.dumps({
                        'schemaVersion': 1, 'id': image_id, 'settings': {}, 'dependencies': []}))
                    content = {'schemaVersion': 1, 'typeId': 'azure.pbr', 'parameters': {
                        'baseColor': [.8, .8, .8, 1], 'roughness': .75, 'metallic': 0,
                        'emissiveStrength': .5, 'transmission': 0, 'ior': 1.5, 'thickness': 0,
                        'attenuationDistance': 0, 'attenuationColor': [1, 1, 1, 1], 'doubleSided': False},
                        'textures': {'emissive': {'asset': image_id, 'usage': 'emissive'}}}
                document.write_text(json.dumps(content))
                document.with_suffix(document.suffix + '.azmeta').write_text(json.dumps({
                    'schemaVersion': 1, 'id': identity, 'settings': {}, 'dependencies': []}))
                level_path = root / 'assets/scene.azurelevel'
                level = json.loads(level_path.read_text())
                level['nodes'][0]['materialSlots'] = {'surface': identity}
                level_path.write_text(json.dumps(level))
            command = [str(player.resolve()), '--project', str(project.resolve()),
                '--width', '128', '--height', '128', '--capture-dir', str(root / 'frames'),
                '--capture-frames', '1', '--fixed-frame-step', '--qa-camera', 'scene',
                '--qa-light', 'scene', '--qa-isolation', 'emissive',
                '--runtime-report', str(root / 'runtime.json')]
            if backend == 'fixed':
                command += ['--disable-bindless', '--disable-compute-skinning', '--disable-parallel-recording']
            (root / 'command.json').write_text(json.dumps(command))
            with (root / 'player.log').open('w') as log:
                subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=120)
            log = (root / 'player.log').read_text(errors='replace')
            assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]', log), key
            assert ('discards source semantics' in log) == (mode == 'replace'), key
            assert f'Character texture access: {backend}' in log, key
            assert 'Allocator after unload: buffers=0 images=0' in log, key
            runtime = json.loads((root / 'runtime.json').read_text())
            assert not runtime['scriptErrors'] and not runtime['presentationErrors'], key
            frame = root / 'frames/frame_000000.png'
            images[key] = np.asarray(Image.open(frame).convert('RGB'))
            expected = linear(np.array([100, 140, 180])) * (.5 if mode != 'zero' else 0)
            error = float(np.abs(linear(images[key][ROI]) - expected).max())
            results.append(dict(name=key, maximumLinearError=error,
                imageSha256=hashlib.sha256(frame.read_bytes()).hexdigest()))
            (output / 'progress.json').write_text(json.dumps(results, indent=2))
            assert error < .004, (key, error)
            print(key, flush=True)
        assert np.array_equal(images[f'{backend}-source'], images[f'{backend}-override']), backend
        assert np.array_equal(images[f'{backend}-source'], images[f'{backend}-replace']), backend
        assert np.any(images[f'{backend}-override'] != images[f'{backend}-zero']), backend
    for mode in ('source', 'override', 'replace', 'zero'):
        assert np.array_equal(images[f'indexed-{mode}'], images[f'fixed-{mode}']), mode
    summary = dict(status='passed', captures=results, maximumLinearErrorBudget=.004,
        playerSha256=hashlib.sha256(player.read_bytes()).hexdigest(),
        scriptSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest())
    (output / 'summary.json').write_text(json.dumps(summary, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    run(args.executable, args.output)
