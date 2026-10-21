"""Verify independent face maps, head animation and descriptor fallback in Player."""
import argparse
import base64
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
