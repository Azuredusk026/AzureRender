"""Measure a local reflection volume through real Player GPU sampling."""
import argparse
import json
from pathlib import Path
import struct
import subprocess

from native_render.light_lab import generate


def environment(path, local=False):
    width, height = 256, 128
    payload = bytearray(b'AZENV001' + struct.pack('<III', width, height, 8))
    for mip in range(8):
        for _ in range(height):
            for x in range(width):
                color = (0, .4, 0, 1)
                if local:
                    # Two sharp levels keep the renderer's minimum roughness
                    # away from the blue coarse levels during projection checks.
                    color = (0, 0, .6, 1) if mip >= 2 or (x + .5) / width > .8 else (.4, 0, 0, 1)
                payload.extend(struct.pack('<4e', *color))
        width, height = max(1, width // 2), max(1, height // 2)
    path.write_bytes(payload)


def run(player, output):
    from PIL import Image
    output.mkdir(parents=True, exist_ok=False)
    project = output / 'project'
    generate(project, 'dark')
    environment(project / 'global.azenv')
    environment(project / 'local.azenv', True)
    base = json.loads((project / 'level.azurelevel').read_text())
    base['renderSettings']['skyIntensity'] = 1
    model = json.loads((project / 'plane.gltf').read_text())
    pbr = model['materials'][0]['pbrMetallicRoughness']
    pbr['metallicFactor'] = 1
    results = {}
    for name in ('global', 'local', 'half-intensity', 'edge', 'outside', 'base-mip', 'projected', 'legacy'):
        pbr['roughnessFactor'] = 0 if name in ('base-mip', 'projected') else 1
        (project / 'plane.gltf').write_text(json.dumps(model))
        level = json.loads(json.dumps(base))
        if name != 'global':
            level['reflectionProbe'] = {'version': 1, 'texture': 'assets:/local.azenv',
                'center': [-3.5, 0, 0] if name == 'edge' else ([10, 0, 0] if name == 'outside' else [0, 0, 0]),
                'halfExtents': [4, 4, 4], 'capturePosition': [3, 0, 0],
                'intensity': .5 if name == 'half-intensity' else 1,
                'blendDistance': 1, 'boxProjection': name == 'projected'}
        (project / 'level.azurelevel').write_text(json.dumps(level))
        folder = output / name
        folder.mkdir()
        command = [str(player.resolve()), '--project', str(project / 'lab.azureproject'),
            '--environment', str(project / 'global.azenv'), '--width', '128', '--height', '128',
            '--capture-dir', str(folder / 'frames'), '--capture-frames', '1', '--capture-fps', '30',
            '--fixed-frame-step', '--runtime-report', str(folder / 'runtime.json'),
            '--qa-camera', 'scene', '--qa-light', 'scene', '--qa-isolation', 'reflection']
        if name == 'legacy':
            command.append('--disable-bindless')
        startup = None
        if hasattr(subprocess, 'STARTUPINFO'):
            startup = subprocess.STARTUPINFO()
            startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
            startup.wShowWindow = 0
        with (folder / 'player.log').open('w') as log:
            subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True,
                           timeout=90, startupinfo=startup)
        log = (folder / 'player.log').read_text(errors='replace')
        assert 'VUID-' not in log and 'Validation Error' not in log, name
        assert 'Allocator after unload: buffers=0 images=0' in log, name
        report = json.loads((folder / 'runtime.json').read_text())
        assert not report['presentationErrors'] and not report['scriptErrors'], name
        pixel = Image.open(folder / 'frames/frame_000000.png').convert('RGB').getpixel((64, 64))
        linear = [v / 255 / 12.92 if v / 255 <= .04045 else ((v / 255 + .055) / 1.055) ** 2.4 for v in pixel]
        results[name] = {'pixel': pixel, 'linear': linear}
    expected = {'global': [0, .4, 0], 'local': [0, 0, .6],
        'half-intensity': [0, 0, .3], 'edge': [0, .2125, .28125],
        'outside': [0, .4, 0], 'base-mip': [.4, 0, 0], 'projected': [0, 0, .6], 'legacy': [0, 0, .6]}
    for name, target in expected.items():
        results[name]['max_error'] = max(abs(a-b) for a, b in zip(results[name]['linear'], target))
    (output / 'metrics.json').write_text(json.dumps(results, indent=2))
    assert all(row['max_error'] < .025 for row in results.values()), results
    return results


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--player', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(run(args.player, args.output.resolve()), indent=2))
