"""Exercise actual scene snapshot selection and preserve an isolated surface image."""
import argparse
import json
from pathlib import Path
import re
import subprocess

import numpy as np
from PIL import Image
from test_character_probe_gpu import prepare


def run(player, output):
    output.mkdir(parents=True, exist_ok=False)
    report = dict(schemaVersion=1, cases=[])
    for aa in [0, 4]:
        images = []
        for enabled in [False, True]:
            folder = output / f'aa-{aa}-reflection-{int(enabled)}'
            folder.mkdir()
            project = prepare(folder / 'project', dict(name='reflection-surface', material='pbr', probe=False))
            mesh_path = project.parent / 'assets/surface.gltf'
            mesh = json.loads(mesh_path.read_text())
            mesh['materials'][0]['emissiveFactor'] = [.2, .04, .01]
            mesh_path.write_text(json.dumps(mesh))
            level_path = project.parent / 'assets/scene.azurelevel'
            level = json.loads(level_path.read_text())
            level['renderSettings'].update(schemaVersion=20, antiAliasing=aa,
                screenSpaceReflection=dict(schemaVersion=1, enabled=enabled))
            level_path.write_text(json.dumps(level, indent=2))
            command = [str(player.resolve()), '--project', str(project.resolve()),
                '--width', '128', '--height', '128', '--capture-dir', str((folder/'frames').resolve()),
                '--capture-frames', '1', '--fixed-frame-step', '--qa-camera', 'scene', '--qa-light', 'scene']
            (folder/'command.json').write_text(json.dumps(command, indent=2))
            startup = None
            if hasattr(subprocess, 'STARTUPINFO'):
                startup = subprocess.STARTUPINFO()
                startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
                startup.wShowWindow = 0
            with (folder/'player.log').open('w') as log:
                subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True,
                    timeout=240, startupinfo=startup)
            log = (folder/'player.log').read_text(errors='replace')
            assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]', log), folder
            assert 'Allocator after unload: buffers=0 images=0' in log, folder
            manifest = json.loads((folder/'frames/capture_manifest.json').read_text())
            assert manifest['reflectionSurfaceSnapshot'] == enabled, folder
            images.append(np.asarray(Image.open(folder/'frames/frame_000000.png').convert('RGB')))
            assert images[-1].max() > 20, 'A visible surface must be rendered'
            report['cases'].append(dict(antiAliasing=aa, requested=enabled,
                surfaceMetadata=manifest['reflectionSurfaceSnapshot'], validationErrors=0, resourcesReleased=True))
        assert np.array_equal(*images), 'A rough isolated surface must preserve its scene image'
    report['status'] = 'passed'
    (output/'summary.json').write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--player', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    run(args.player, args.output)
