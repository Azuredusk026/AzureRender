"""Verify scene-preserving component diagnostics through the real Player."""
import argparse
import json
import subprocess
from pathlib import Path

from native_render.light_lab import generate


def run(player, output):
    from PIL import Image, ImageChops
    output.mkdir(parents=True, exist_ok=False)
    project = output / 'project'
    generate(project, 'point')
    material_path = project / 'plane.gltf'
    material = json.loads(material_path.read_text())
    pbr = material['materials'][0]['pbrMetallicRoughness']
    pbr.update(baseColorFactor=[.25, .36, .49, 1], metallicFactor=.25, roughnessFactor=.6)
    material_path.write_text(json.dumps(material), encoding='utf-8')
    pictures = {}
    metrics = {}
    for name in ('baseline', 'beauty', 'albedo', 'roughness', 'metallic',
                 'direct-specular', 'reflection', 'shaded-normal'):
        folder = output / name
        folder.mkdir()
        command = [str(player.resolve()), '--project', str(project / 'lab.azureproject'),
                   '--width', '128', '--height', '128', '--capture-dir', str(folder / 'frames'),
                   '--capture-frames', '1', '--capture-fps', '30', '--fixed-frame-step',
                   '--runtime-report', str(folder / 'runtime.json')]
        if name != 'baseline':
            command += ['--qa-camera', 'scene', '--qa-light', 'scene', '--qa-isolation', name]
        startup = None
        if hasattr(subprocess, 'STARTUPINFO'):
            startup = subprocess.STARTUPINFO()
            startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
            startup.wShowWindow = 0
        with (folder / 'player.log').open('w', encoding='utf-8') as log:
            subprocess.run(command, stdout=log, stderr=subprocess.STDOUT,
                           timeout=120, check=True, startupinfo=startup)
        log = (folder / 'player.log').read_text(encoding='utf-8', errors='replace')
        if 'VUID-' in log or 'Validation Error' in log:
            raise AssertionError(f'Vulkan validation failure: {folder}')
        if 'Allocator after unload: buffers=0 images=0' not in log:
            raise AssertionError(f'Missing resource-release receipt: {folder}')
        runtime = json.loads((folder / 'runtime.json').read_text())
        if runtime['presentationErrors'] or runtime['scriptErrors']:
            raise AssertionError(f'Runtime errors: {folder}')
        capture = json.loads((folder / 'frames/capture_manifest.json').read_text())
        pictures[name] = Image.open(folder / 'frames/frame_000000.png').convert('RGB')
        metrics[name] = {'center': pictures[name].getpixel((64, 64)),
                         'grade': capture['gradeSettings'],
                         'camera': capture['qaCameraPosition'],
                         'target': capture['qaCameraTarget'],
                         'antiAliasing': capture['antiAliasing']}
    def linear(value):
        value /= 255
        return value / 12.92 if value <= .04045 else ((value + .055) / 1.055) ** 2.4
    assert ImageChops.difference(pictures['baseline'], pictures['beauty']).getbbox() is None, metrics
    for name in pictures:
        for field in ('grade', 'camera', 'target', 'antiAliasing'):
            assert metrics[name][field] == metrics['baseline'][field], (name, field, metrics)
    for name, expected in [('albedo', [.25, .36, .49]), ('roughness', [.6] * 3),
                           ('metallic', [.25] * 3), ('shaded-normal', [.5, .5, 1])]:
        errors = [abs(linear(value) - target)
                  for value, target in zip(metrics[name]['center'], expected)]
        metrics[name]['linear_errors'] = errors
        assert max(errors) < .01, (name, errors)
    assert max(metrics['direct-specular']['center']) > 0, metrics
    assert max(high for low, high in pictures['reflection'].getextrema()) == 0, metrics
    (output / 'metrics.json').write_text(json.dumps(metrics, indent=2), encoding='utf-8')
    return metrics


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--player', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(run(args.player, args.output.resolve()), indent=2))


if __name__ == '__main__':
    main()
