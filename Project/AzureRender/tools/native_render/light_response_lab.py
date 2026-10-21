"""Measure per-light specular scaling on metallic and mixed public surfaces."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

from light_lab import generate


def run(player, output, tracing=False):
    import numpy as np
    from PIL import Image
    metrics = {}
    for surface, metal in [('metal', 1), ('mixed', .3)]:
        frames = {}
        bounds = {}
        for name, scale in [('zero', 0), ('native', .195), ('full', 1)]:
            folder = output / (surface + '-' + name)
            generate(folder, 'directional-near')
            file = folder / 'plane.gltf'
            document = json.loads(file.read_text())
            document['materials'][0]['pbrMetallicRoughness'].update(
                baseColorFactor=[.6, .6, .6, 1], metallicFactor=metal, roughnessFactor=.6)
            file.write_text(json.dumps(document))
            file = folder / 'level.azurelevel'
            document = json.loads(file.read_text())
            document['lights'][0]['shape']['specularScale'] = scale
            if tracing:
                document['renderSettings'].update(renderer='pathtracing', pathTraceSamples=64,
                                                  pathTraceBounces=2, pathTraceSeed=7)
            file.write_text(json.dumps(document, indent=2))
            command = [str(player), '--project', str(folder/'lab.azureproject'),
                       '--width', '128', '--height', '128', '--runtime-report', str(folder/'runtime.json')]
            if tracing:
                script = {'schemaVersion': 1, 'steps': [
                    {'op': 'wait-frames', 'frames': 32, 'timeoutMs': 60000},
                    {'op': 'wait-frames', 'frames': 32, 'timeoutMs': 60000},
                    {'op': 'wait-frames', 'frames': 5}, {'op': 'screenshot', 'label': 'final'}]}
                (folder/'steps.json').write_text(json.dumps(script))
                command += ['--resource-root', str(folder), '--validation-script', str(folder/'steps.json'),
                            '--validation-report', str(folder/'validation.json'), '--smoke-frames', '100']
                capture = folder/'captures/final.png'
            else:
                command += ['--capture-dir', str(folder/'frames'), '--capture-frames', '1',
                            '--capture-fps', '30', '--fixed-frame-step']
                capture = folder/'frames/frame_000000.png'
            (folder/'command.json').write_text(json.dumps(command, indent=2))
            with (folder/'player.log').open('w') as log:
                subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=180)
            log = (folder/'player.log').read_text(errors='replace')
            assert not re.search(r'VUID-|Validation Error|\[error\]', log)
            assert 'Allocator after unload: buffers=0 images=0' in log
            runtime = json.loads((folder/'runtime.json').read_text())
            assert not runtime['scriptErrors'] and not runtime['presentationErrors']
            values = np.asarray(Image.open(capture).convert('RGB'), dtype=float)[16:-16,16:-16] / 255
            def linear(encoded):
                return np.where(encoded <= .04045, encoded/12.92, ((encoded+.055)/1.055)**2.4)
            frames[name] = linear(values)
            bounds[name] = (linear(np.maximum(0, values-.5/255)),
                            linear(np.minimum(1, values+.5/255)))
        specular = frames['full'] - frames['zero']
        valid = specular > .03
        assert valid.sum() > 100, 'Fixture needs a measurable direct specular response'
        error = np.abs((frames['native']-frames['zero'])[valid]/specular[valid] - .195)
        # Each stored byte represents an interval. Compare the predicted linear
        # response interval with the actual interval before normalizing error.
        lower = .805*bounds['zero'][0] + .195*bounds['full'][0]
        upper = .805*bounds['zero'][1] + .195*bounds['full'][1]
        residual = np.maximum(0, np.maximum(lower-bounds['native'][1], bounds['native'][0]-upper))
        corrected = residual[valid] / specular[valid]
        metrics[surface] = dict(maximumScaleError=float(error.max()), meanScaleError=float(error.mean()),
                                maximumScaleErrorBeyondQuantization=float(corrected.max()),
                                diffuseMean=float(frames['zero'].mean()), samples=int(valid.sum()))
        assert corrected.max() < .025, metrics[surface]
        if surface == 'metal':
            assert frames['zero'].max() < .001, 'Pure metal must have no diffuse contribution'
        else:
            assert frames['zero'].mean() > .01, 'Mixed surface must retain its diffuse contribution'
    result = dict(playerSha256=hashlib.sha256(player.read_bytes()).hexdigest(), tracing=tracing,
                  tolerance=.025, surfaces=metrics)
    (output/'metrics.json').write_text(json.dumps(result, indent=2))
    print(json.dumps(result))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--player', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--tracing', action='store_true')
    args = parser.parse_args()
    run(args.player.resolve(), args.output.resolve(), args.tracing)
