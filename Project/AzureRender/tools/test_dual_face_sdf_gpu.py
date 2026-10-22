"""Verify independent face maps, head animation and descriptor fallback in Player."""
import argparse
import base64
import copy
import io
import json
import math
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import numpy as np
from PIL import Image
from create_visibility_fixture import surface, component


def run_normal_checks(executable, root):
    root.mkdir(parents=True, exist_ok=True)
    assets = root / 'assets'
    assets.mkdir(exist_ok=False)
    surface(assets / 'surface.gltf', double_sided=True)
    base = json.loads((assets / 'surface.gltf').read_text())
    uv = struct.pack('<8f', 0, 0, 1, 0, 1, 1, 0, 1)
    buffer = len(base['buffers'])
    base['buffers'].append({'byteLength': len(uv), 'uri': 'data:application/octet-stream;base64,' + base64.b64encode(uv).decode()})
    view = len(base['bufferViews'])
    base['bufferViews'].append({'buffer': buffer, 'byteLength': len(uv)})
    accessor = len(base['accessors'])
    base['accessors'].append({'bufferView': view, 'componentType': 5126, 'count': 4, 'type': 'VEC2'})
    base['meshes'][0]['primitives'][0]['attributes']['TEXCOORD_0'] = accessor
    base['nodes'].append({'name': 'FaceHead'})
    base['scenes'][0]['nodes'].append(1)
    frames = {}

    def render(name, blue, mask=0, angle=0, scene=False, isolation='rim', fixed=False, directional=False, sdf_enabled=True, dark=False, intensity=8):
        model = copy.deepcopy(base)
        model['materials'][0]['pbrMetallicRoughness'].update(baseColorFactor=[.8, .8, .8, 1], roughnessFactor=.6)
        model['nodes'][1]['rotation'] = [0, math.sin(math.radians(angle)/2), 0, math.cos(math.radians(angle)/2)]
        def image(pixels):
            encoded = io.BytesIO()
            Image.fromarray(np.array(pixels, dtype=np.uint8), 'RGBA').save(encoded, format='PNG')
            model.setdefault('images', []).append({'uri': 'data:image/png;base64,' + base64.b64encode(encoded.getvalue()).decode()})
            index = len(model.setdefault('textures', []))
            model['textures'].append({'source': len(model['images']) - 1})
            return index
        sdf = image([[[128, 128, blue, 255]]])
        mask_texture = image([[[0, mask, 0, 255]]])
        ramp = image([[[round(x*255/31)]*3 + [255] for x in range(32)]])
        model['materials'][0]['extras'] = {'azureRenderMaterial': {
            'schemaVersion': 2, 'class': 'face', 'features': ['face-sdf-eligible'],
            'styleParameters': [1, 1, 1, 1],
            'faceSdf': {'schemaVersion': 2, 'texture': sdf, 'maskTexture': mask_texture, 'rampTexture': ramp,
                'texCoord': 0, 'headNode': 'FaceHead', 'headBasis': [1, 0, 0, 0, 1, 0, 0, 0, 1]}}}
        (assets / (name + '.gltf')).write_text(json.dumps(model))
        level = {'schemaVersion': 1, 'id': name, 'sceneType': 'character',
            'renderSettings': {'platform': False, 'silhouetteOutline': False, 'innerOutline': False,
                'stylized': True, 'bloomEnabled': False, 'antiAliasing': 0, 'exposure': 0, 'saturation': 1,
                'contrast': 1, 'toneMapping': False, 'sceneLighting': scene, 'skyIntensity': 0, 'faceSdf': sdf_enabled},
            'resources': [{'id': 'face', 'asset': 'assets:/' + name + '.gltf'}],
            'nodes': [{'id': 'face', 'resourceId': 'face'}, {'id': 'camera', 'components': {
                'azure.transform': component('azure.transform', {'translation': [0, 0, 3]}),
                'azure.camera': component('azure.camera', {'enabled': True, 'priority': 1, 'fieldOfView': 55})}}]}
        if scene:
            level['nodes'].append({'id': 'key', 'components': {
                'azure.transform': component('azure.transform', {'translation': [3, 0, 2]})}})
            light = {'id': 'key', 'nodeId': 'key', 'color': [1, 1, 1], 'intensity': intensity, 'radius': 10, 'enabled': not dark}
            if directional:
                light['shape'] = {'type': 'directional', 'direction': [0, 0, -1]}
            level['lights'] = [light]
        (assets / (name + '.azurelevel')).write_text(json.dumps(level))
        project = root / (name + '.azureproject')
        project.write_text(json.dumps({'schemaVersion': 1, 'id': name, 'name': name,
            'mounts': [{'name': 'assets', 'path': 'assets'}], 'startupScene': 'assets:/' + name + '.azurelevel'}))
        output = root / name
        output.mkdir()
        command = [str(executable.resolve()), '--project', str(project.resolve()), '--width', '320', '--height', '240',
            '--capture-dir', str((output / 'frames').resolve()), '--capture-frames', '1', '--fixed-frame-step',
            '--qa-camera', 'scene', '--qa-light', 'scene', '--qa-isolation', isolation,
            '--runtime-report', str((output / 'runtime.json').resolve())]
        if fixed:
            command += ['--disable-bindless', '--disable-compute-skinning', '--disable-parallel-recording']
        (output / 'command.json').write_text(json.dumps(command, indent=2))
        with (output / 'player.log').open('w', encoding='utf8') as log:
            subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=120)
        log = (output / 'player.log').read_text(encoding='utf8', errors='replace')
        assert 'Validation layer: enabled' in log and not re.search(r'VUID-|Validation Error|\[error\]', log), name
        assert 'Allocator after unload: buffers=0 images=0' in log, name
        report = json.loads((output / 'runtime.json').read_text())
        assert not report['scriptErrors'] and not report['presentationErrors'], name
        frames[name] = np.asarray(Image.open(output / 'frames/frame_000000.png').convert('RGB'), dtype=np.int16)
        return frames[name][100:140, 140:180]

    metrics = {}
    rim_low = render('rim-low', 32)
    rim_high = render('rim-high', 223)
    metrics['blueRimMeanDifference'] = float(np.abs(rim_low-rim_high).mean())
    assert metrics['blueRimMeanDifference'] > 1, ('B channel must reach production rim', metrics)
    masked_low = render('rim-mask-low', 32, mask=255)
    masked_high = render('rim-mask-high', 223, mask=255)
    assert np.array_equal(masked_low, masked_high), 'Geometry mask must suppress B-normal rim changes'
    head = render('rim-head60', 223, angle=60)
    metrics['headRimMean'] = float(head.mean())
    metrics['baseRimMean'] = float(rim_high.mean())
    assert metrics['headRimMean'] < metrics['baseRimMean'] * .2, 'Head-frame direction or main-light mirror is incorrect'
    render('rim-fixed', 223, fixed=True)
    assert np.array_equal(frames['rim-high'], frames['rim-fixed']), 'Fixed and indexed normal/rim paths differ'
    local_low = render('local-low', 32, mask=128, scene=True, isolation='direct-diffuse')
    local_high = render('local-high', 223, mask=128, scene=True, isolation='direct-diffuse')
    metrics['blueLocalDiffuseMeanDifference'] = float(np.abs(local_low-local_high).mean())
    assert metrics['blueLocalDiffuseMeanDifference'] > 1, 'B channel must reach punctual NdotL'
    local_mask_low = render('local-mask-low', 32, mask=255, scene=True, isolation='direct-diffuse')
    local_mask_high = render('local-mask-high', 223, mask=255, scene=True, isolation='direct-diffuse')
    assert np.array_equal(local_mask_low, local_mask_high), 'Geometry mask must suppress B-normal punctual changes'
    render('local-fixed', 223, mask=128, scene=True, isolation='direct-diffuse', fixed=True)
    assert np.array_equal(frames['local-high'], frames['local-fixed']), 'Fixed and indexed punctual normal paths differ'
    reference8 = render('main-specular-reference8', 32, mask=128, scene=True, isolation='direct-specular', directional=True)
    main_low = render('main-specular-low', 32, mask=128, scene=True, isolation='direct-specular', directional=True, intensity=16)
    main_high = render('main-specular-high', 128, mask=128, scene=True, isolation='direct-specular', directional=True, intensity=16)
    main_head = render('main-specular-head', 32, mask=128, angle=60, scene=True, isolation='direct-specular', directional=True, intensity=16)
    assert main_low.max() > 5, 'Main specular stability test must contain a visible contribution'
    assert np.array_equal(main_low, main_high) and np.array_equal(main_low, main_head), 'SDF normal must preserve main geometric specular'
    # Independent peak bounds: .08*.22*mix(.7,.12,.6)*.03*intensity.
    # These controls preserve the >5 predicate and geometric-SDF equality.
    def linear(pixels):
        encoded = pixels.astype(float) / 255
        return np.where(encoded <= .04045, encoded / 12.92,
                        ((encoded + .055) / 1.055) ** 2.4)
    reference_linear = linear(reference8)
    strong_linear = linear(main_low)
    quantum = 1 / (255 * 12.92)  # Both peaks lie in the linear sRGB segment.
    assert abs(reference_linear.max() - .001486848) <= quantum, 'Intensity8 analytic peak mismatch'
    assert abs(strong_linear.max() - .002973696) <= quantum, 'Intensity16 analytic peak mismatch'
    assert main_low.max() > reference8.max(), 'Directional intensity must increase specular'
    pair_error = float(np.abs(strong_linear - 2 * reference_linear).max())
    assert pair_error <= 1.5 * quantum + 1e-6, 'Directional 8-to-16 linear response mismatch'
    metrics['mainSpecularReference8Maximum'] = int(reference8.max())
    metrics['mainSpecularIntensity16Maximum'] = int(main_low.max())
    metrics['mainSpecularLinearDoublingError'] = pair_error
    disabled_low = render('disabled-low', 32, sdf_enabled=False)
    disabled_high = render('disabled-high', 223, sdf_enabled=False)
    assert np.array_equal(disabled_low, disabled_high), 'Disabled SDF must preserve geometric response'
    dark = render('local-dark', 223, mask=128, scene=True, isolation='direct-diffuse', dark=True)
    assert dark.max() == 0, 'Normal transport must not create light energy'
    summary = {'frames': len(frames), 'validationErrors': 0, 'unloadResources': 0, 'metrics': metrics,
        'fixedDifferences': 0, 'geometryMaskDifferences': 0, 'mainSpecularDifferences': 0,
        'disabledDifferences': 0, 'mainSpecularMaximum': int(main_low.max()),
        'scope': 'Native B-normal transport and selected production consumers; full native rim/light models remain pending'}
    (root / 'summary.json').write_text(json.dumps(summary, indent=2))
    return summary


def run(executable, root):
    assets = root / 'assets'
    assets.mkdir(parents=True, exist_ok=False)
    surface(assets / 'face.gltf', double_sided=True)
    model = json.loads((assets / 'face.gltf').read_text())
    model['materials'][0]['pbrMetallicRoughness']['baseColorFactor'] = [.8, .8, .8, 1]
    def accessor(values, kind, count):
        data = struct.pack('<' + 'f' * len(values), *values)
        buffer = len(model['buffers'])
        model['buffers'].append({'byteLength': len(data), 'uri': 'data:application/octet-stream;base64,' + base64.b64encode(data).decode()})
        view = len(model['bufferViews'])
        model['bufferViews'].append({'buffer': buffer, 'byteLength': len(data)})
        index = len(model['accessors'])
        model['accessors'].append({'bufferView': view, 'componentType': 5126, 'count': count, 'type': kind})
        if kind == 'SCALAR':
            model['accessors'][-1].update(min=[min(values)], max=[max(values)])
        return index
    model['meshes'][0]['primitives'][0]['attributes']['TEXCOORD_0'] = accessor([0, 0, 1, 0, 1, 1, 0, 1], 'VEC2', 4)
    head = len(model['nodes'])
    model['nodes'].append({'name': 'FaceHead'})
    model['scenes'][0]['nodes'].append(head)
    times = accessor([0, 1], 'SCALAR', 2)
    rotations = accessor([0, 0, 0, 1, 0, 1, 0, 0], 'VEC4', 2)
    animations = [{'name': 'head-turn', 'samplers': [{'input': times, 'output': rotations, 'interpolation': 'LINEAR'}],
        'channels': [{'sampler': 0, 'target': {'node': head, 'path': 'rotation'}}]}]
    def image(pixels):
        encoded = io.BytesIO()
        Image.fromarray(np.array(pixels, dtype=np.uint8), 'RGBA').save(encoded, format='PNG')
        model.setdefault('images', []).append({'uri': 'data:image/png;base64,' + base64.b64encode(encoded.getvalue()).decode()})
        index = len(model.setdefault('textures', []))
        model['textures'].append({'source': len(model['images']) - 1})
        return index
    sdf = image([[[0, 0, 128, 255], [255, 255, 128, 255]]])
    mask = image([[[0, 0, 0, 255]]])
    ramp = image([[[round(x * 255 / 31), 0, 255 - round(x * 255 / 31), 255] for x in range(32)]])
    # The default scene key has horizontal direction (3,0,2).
    x, z = 3 / math.sqrt(13), 2 / math.sqrt(13)
    profile = {'schemaVersion': 2, 'class': 'face', 'features': ['face-sdf-eligible'],
        'faceSdf': {'schemaVersion': 2, 'texture': sdf, 'maskTexture': mask, 'rampTexture': ramp,
            'texCoord': 0, 'headNode': 'FaceHead', 'headBasis': [x, 0, z, 0, 1, 0, -z, 0, x]}}
    model['materials'][0]['extras'] = {'azureRenderMaterial': profile}
    level = {'schemaVersion': 1, 'id': 'dual-face', 'sceneType': 'character',
        'renderSettings': {'platform': False, 'silhouetteOutline': False, 'innerOutline': False,
            'stylized': True, 'bloomEnabled': False, 'antiAliasing': 0, 'exposure': 0, 'contrast': 1},
        'resources': [{'id': 'face', 'asset': 'assets:/face.gltf'}],
        'nodes': [{'id': 'face', 'resourceId': 'face'}, {'id': 'camera', 'components': {
            'azure.transform': component('azure.transform', {'translation': [0, 0, 3]}),
            'azure.camera': component('azure.camera', {'enabled': True, 'priority': 1, 'fieldOfView': 55})}}]}
    project = root / 'project.azureproject'
    project.write_text(json.dumps({'schemaVersion': 1, 'id': 'dual-face', 'name': 'Dual face contract',
        'mounts': [{'name': 'assets', 'path': 'assets'}], 'startupScene': 'assets:/scene.azurelevel'}))
    results = {}
    for mode in ['head0', 'head180', 'fixed', 'mask', 'animated',
                 'scene-head0', 'scene-head180', 'scene-mask', 'scene-fixed', 'scene-dark']:
        scene_mode = mode.startswith('scene-')
        level['renderSettings'].update(sceneLighting=scene_mode, skyIntensity=0)
        level['lights'] = ([{'id': 'key', 'nodeId': 'key', 'color': [1, 1, 1],
            'intensity': 8, 'radius': 10, 'enabled': mode != 'scene-dark'}] if scene_mode else [])
        level['nodes'] = level['nodes'][:2]
        if scene_mode:
            level['nodes'].append({'id': 'key', 'components': {
                'azure.transform': component('azure.transform', {'translation': [3, 0, 2]})}})
        model.pop('animations', None)
        if mode == 'animated':
            model['animations'] = animations
        model['nodes'][head]['rotation'] = [0, 1, 0, 0] if mode in ('head180', 'scene-head180') else [0, 0, 0, 1]
        if mode in ('mask', 'scene-mask'):
            profile['faceSdf']['maskTexture'] = sdf
        else:
            profile['faceSdf']['maskTexture'] = mask
        level['nodes'][0]['components'] = {}
        if mode == 'animated':
            (assets / 'motion.json').write_text(json.dumps({'schemaVersion': 1, 'initial': 'turn',
                'states': [{'name': 'turn', 'clip': 0}], 'transitions': []}))
            level['nodes'][0]['components']['azure.animator'] = component('azure.animator', {'asset': 'assets:/motion.json', 'state': 'turn', 'startTime': .6})
        (assets / 'face.gltf').write_text(json.dumps(model))
        (assets / 'scene.azurelevel').write_text(json.dumps(level))
        output = root / mode
        command = [str(executable.resolve()), '--project', str(project.resolve()), '--width', '320', '--height', '240',
            '--capture-dir', str(output.resolve()), '--capture-frames', '1', '--capture-fps', '4',
            '--qa-light', 'scene',
            '--runtime-report', str((root / (mode + '-runtime.json')).resolve())]
        if not scene_mode:
            command += ['--qa-isolation', 'diffuse-band']
        if mode in ('fixed', 'scene-fixed'):
            command += ['--disable-bindless', '--disable-compute-skinning', '--disable-parallel-recording']
        result = subprocess.run(command, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=120)
        (root / (mode + '.log')).write_text(result.stdout + result.stderr, encoding='utf-8')
        assert result.returncode == 0 and not re.search(r'VUID-|Validation Error|\[error\]', result.stdout + result.stderr), mode
        assert 'Validation layer: enabled' in result.stdout, mode
        assert 'Allocator after unload: buffers=0 images=0' in result.stdout, mode
        data = json.loads((root / (mode + '-runtime.json')).read_text())
        assert not data['scriptErrors'] and not data['presentationErrors']
        results[mode] = np.asarray(Image.open(output / 'frame_000000.png').convert('RGB'), dtype=np.int16)
    head = float(np.abs(results['head0'] - results['head180']).mean())
    mask_response = float(np.abs(results['head0'] - results['mask']).mean())
    animation = float(np.abs(results['head0'] - results['animated']).mean())
    fallback = float(np.abs(results['head0'] - results['fixed']).mean())
    assert min(head, mask_response, animation) > 1 and fallback < .1, (head, mask_response, animation, fallback)
    scene_head = float(np.abs(results['scene-head0'] - results['scene-head180']).mean())
    scene_mask = float(np.abs(results['scene-head0'] - results['scene-mask']).mean())
    scene_fallback = float(np.abs(results['scene-head0'] - results['scene-fixed']).mean())
    # Ignore the host clear color outside the planar surface.
    scene_dark = int(results['scene-dark'][85:155, 125:195].max())
    assert min(scene_head, scene_mask) > 1, ('scene face response missing', scene_head, scene_mask)
    assert scene_fallback < .1 and scene_dark == 0, (scene_fallback, scene_dark)
    summary = {'frames': len(results), 'headRotationResponse': head, 'independentMaskResponse': mask_response,
        'sceneHeadRotationResponse': scene_head, 'sceneIndependentMaskResponse': scene_mask,
        'sceneFixedDescriptorCpuDifference': scene_fallback, 'sceneDarkMaximum': scene_dark,
        'headAnimationResponse': animation, 'fixedDescriptorCpuDifference': fallback, 'validationErrors': 0}
    summary['normal'] = run_normal_checks(executable, root / 'blue-normal')
    (root / 'summary.json').write_text(json.dumps(summary, indent=2))
    print(json.dumps(summary))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', required=True, type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if args.output:
        run(args.executable, args.output.resolve())
    else:
        runs = Path(__file__).resolve().parents[1] / 'build/runs'
        with tempfile.TemporaryDirectory(prefix='dual-face-', dir=runs) as temporary:
            run(args.executable, Path(temporary))
