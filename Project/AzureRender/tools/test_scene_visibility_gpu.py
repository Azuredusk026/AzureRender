"""Assert single-sided winding, clip distance and depth on Vulkan."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile
import numpy as np
from PIL import Image
from create_visibility_fixture import create


def capture(executable, root, name, *, flags=(), isolation='albedo', disable_outline=True, **fixture):
    project = create(root / name, **fixture)
    output = root / (name + '-capture')
    command = [str(executable), '--project', str(project), '--width', '960', '--height', '540',
               '--qa-camera', 'full-body-front', '--qa-light', 'neutral-material',
               '--qa-isolation', isolation,
               '--capture-dir', str(output), '--capture-frames', '1', '--fixed-frame-step', *flags]
    if disable_outline: command += ['--qa-effect', 'outline', '--qa-effect-state', 'disabled']
    startup = subprocess.STARTUPINFO(); startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW; startup.wShowWindow = 0
    result = subprocess.run(command, cwd=executable.parent, capture_output=True, encoding='utf-8', errors='replace', timeout=120, startupinfo=startup)
    log = result.stdout + result.stderr
    (root / (name + '.log')).write_text(log, encoding='utf-8')
    assert result.returncode == 0 and 'VUID-' not in log and 'Validation Error' not in log, name + ': ' + log
    assert 'Allocator after unload: buffers=0 images=0' in log, name
    manifest = json.loads((output / 'capture_manifest.json').read_text(encoding='utf-8'))
    assert manifest.get('qaDisableFaceCulling') == ('--qa-disable-face-culling' in flags), 'face diagnostic provenance missing'
    assert manifest.get('qaDisableDepthTest') == ('--qa-disable-depth-test' in flags), 'depth diagnostic provenance missing'
    assert manifest.get('cameraFar') == fixture.get('camera_far', 500), 'camera range provenance missing'
    return np.asarray(Image.open(output / 'frame_000000.png').convert('RGB'), dtype=np.int16)


def run(executable, root):
    root.mkdir(parents=True, exist_ok=True)
    images = {}
    failures = []
    cases = {'positive': {}, 'mirror': {'scale': [-1, 1, 1]}, 'double-mirror': {'scale': [-1, -1, 1]},
             'baked-mirror': {'baked': True}, 'back': {'rotation': [0, 180, 0]},
             'double-sided-back': {'rotation': [0, 180, 0], 'double': True},
             'far': {'far': True}, 'far-clipped': {'far': True, 'camera_far': 100},
             'depth': {'depth': True}, 'transparent': {'alpha': True},
             'transparent-mirror': {'alpha': True, 'scale': [-1, 1, 1]}, 'mixed': {'mixed': True},
             'near': {'offset': [0, 1.1228, 3.3583], 'scale': [.05, .05, .05]},
             'near-clipped': {'offset': [0, 1.1657, 3.502], 'scale': [.01, .01, .01]},
             'edge': {'offset': [2.35, 0, 0]}}
    for name, fixture in cases.items():
        try:
            images[name] = capture(executable, root, name, outline=False, **fixture)
        except AssertionError as error:
            failures.append(str(error))
    def check(condition, message):
        if not condition: failures.append(message)
    def red(image):
        # glTF factors and alpha blending are linear. Test chromatic dominance
        # in that space while retaining the coverage and parity requirements.
        encoded=image.astype(np.float64)/255
        linear=np.where(encoded<=.04045,encoded/12.92,((encoded+.055)/1.055)**2.4)
        return (linear[:,:,0]>linear[:,:,1]*1.5)&(linear[:,:,0]>linear[:,:,2]*1.5)
    counts = {name: int(red(image).sum()) for name, image in images.items()}
    for name in cases: counts.setdefault(name, 0)
    check(counts['positive'] > 1000, 'positive front surface absent')
    for name in ['mirror', 'double-mirror', 'baked-mirror']:
        check(counts[name] >= counts['positive'] * .98, name + ' front surface absent')
    check(counts['back'] < 10, 'single-sided rear face visible')
    check(counts['double-sided-back'] > 1000, 'double-sided rear face absent')
    check(counts['far'] > 100, 'far surface clipped inside authored 500m range')
    check(counts['far-clipped'] < 10, 'far surface visible beyond authored 100m range')
    check(counts['near'] > 100 and counts['near-clipped'] < 10, 'near clip plane differs from authored range')
    check(counts['edge'] > 100, 'partially intersecting frustum surface absent')
    check(int(red(images['mixed'])[:, :480].sum()) > 1000 and int(red(images['mixed'])[:, 480:].sum()) > 1000,
          'mixed transform parity batch dropped an instance')
    center = images['depth'][240:300, 440:520]
    green = (center[:, :, 1] > center[:, :, 0] * 1.5) & (center[:, :, 1] > center[:, :, 2] * 1.5)
    check(float(np.mean(green)) > .95, 'near green occluder absent or overwritten by rear surface')
    check(counts['transparent'] > 1000 and counts['transparent-mirror'] >= counts['transparent']*.98,
          'transparent mirror surface absent')
    face_off = capture(executable, root, 'face-off', flags=('--qa-disable-face-culling',), rotation=[0, 180, 0], outline=False)
    check(int(red(face_off).sum()) > 1000, 'face diagnostic failed to expose rear face')
    depth_off = capture(executable, root, 'depth-off', flags=('--qa-disable-depth-test',), depth=True, outline=False)
    check(float(red(depth_off[240:300, 440:520]).mean()) > .95, 'depth diagnostic failed to expose submitted rear surface')
    normal_baked = capture(executable, root, 'normal-baked', sloped=True, baked_scale=[3, 1, 1], isolation='world-normal', outline=False)
    normal_runtime = capture(executable, root, 'normal-runtime', sloped=True, scale=[3, 1, 1], isolation='world-normal', outline=False)
    normal_delta = float(np.abs(normal_baked[240:300, 440:520] - normal_runtime[240:300, 440:520]).mean())
    check(normal_delta < 1, 'nonuniform runtime normal differs from baked inverse-transpose: ' + str(normal_delta))
    tangent_baked = capture(executable, root, 'tangent-baked', baked=True, normal_map=True, isolation='direct-diffuse', outline=False)
    tangent_runtime = capture(executable, root, 'tangent-runtime', scale=[-1, 1, 1], normal_map=True, isolation='direct-diffuse', outline=False)
    tangent_delta = float(np.abs(tangent_baked[240:300, 440:520] - tangent_runtime[240:300, 440:520]).mean())
    check(tangent_delta < 1, 'runtime mirror tangent handedness differs from baked mirror: ' + str(tangent_delta))
    direct = capture(executable, root, 'direct', flags=('--disable-gpu-culling',), outline=False)
    all_visible = capture(executable, root, 'all-visible', flags=('--disable-culling', '--disable-gpu-culling'), outline=False)
    deltas = [float(np.abs(images['positive'] - image).mean()) for image in [direct, all_visible]]
    check(max(deltas) < .1, 'CPU/GPU visibility output differs')
    for name in ['mixed', 'near', 'edge']:
        reference = capture(executable, root, name+'-cpu', flags=('--disable-gpu-culling',), outline=False, **cases[name])
        check(float(np.abs(reference-images[name]).mean()) < .1, name+' CPU/GPU visibility differs')
    beauty = capture(executable, root, 'beauty', isolation='beauty', disable_outline=False)
    check(float(beauty.std()) > 5, 'default beauty/outline capture is empty')
    result = {'status': 'failed' if failures else 'passed', 'surfacePixels': counts, 'visibilityMeanDifference': deltas,
              'normalMeanDifference': normal_delta, 'tangentMeanDifference': tangent_delta, 'validationErrors': 0, 'failures': failures}
    (root / 'summary.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps(result), flush=True)
    assert not failures, failures
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='azure_visibility_') as temporary:
        run(args.executable.resolve(), args.output.resolve() if args.output else Path(temporary))
