"""Validate node opacity through public glTF, project and Player interfaces."""
import argparse
import base64
import copy
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile

import numpy as np
from PIL import Image
from create_visibility_fixture import surface


def component(kind, data, version=1):
    return {'type': kind, 'version': version, 'data': data}


def animated(model):
    model = copy.deepcopy(model)
    model['extensionsUsed'] = ['AZURE_node_opacity']
    # A STEP curve makes the three instance ages exact and independent of the
    # capture host's elapsed startup time. Geometry uses the same shared mesh.
    binary = struct.pack('<6f', 0, 1, 2, 1, .5, 0)
    buffer = len(model['buffers'])
    model['buffers'].append({'byteLength': len(binary), 'uri':
        'data:application/octet-stream;base64,' + base64.b64encode(binary).decode()})
    views = len(model['bufferViews'])
    model['bufferViews'] += [{'buffer': buffer, 'byteOffset': offset, 'byteLength': 12}
                             for offset in (0, 12)]
    accessors = len(model['accessors'])
    model['accessors'] += [{'bufferView': views + i, 'componentType': 5126,
                            'count': 3, 'type': 'SCALAR'} for i in (0, 1)]
    model['animations'] = [{'name': 'fade', 'channels': [], 'samplers': [{
        'input': accessors, 'output': accessors + 1, 'interpolation': 'STEP'}],
        'extensions': {'AZURE_node_opacity': {'schemaVersion': 1,
            'channels': [{'node': 0, 'sampler': 0}]}}}]
    return model


def run(player, root, modes=('opaque', 'blend', 'depth', 'shadow', 'outline')):
    root.mkdir(parents=True, exist_ok=False)
    assets = root / 'assets'
    assets.mkdir()
    surface(assets / 'sign.gltf', double_sided=True)
    baseline = json.loads((assets / 'sign.gltf').read_text())
    material = baseline['materials'][0]
    material['pbrMetallicRoughness']['baseColorFactor'] = [.9, .12, .04, 1]
    surface(assets / 'receiver.gltf', double_sided=True)
    cube = json.loads((Path(__file__).resolve().parents[1] /
        'assets_public/scene_inspector/assets/cube.gltf').read_text())
    cube['materials'] = copy.deepcopy(baseline['materials'])
    tint = copy.deepcopy(baseline)
    tint['materials'][0].update(alphaMode='BLEND', extras={'azureRenderMaterial': {
        'schemaVersion': 2, 'class': 'generic', 'features': ['scene-tint'],
        'styleParameters': [0, 1, 0, 1], 'featureParameters': [1, 1, 6, 1]}})
    (assets / 'tint.gltf').write_text(json.dumps(tint))
    (assets / 'motion.json').write_text(json.dumps({'schemaVersion': 1, 'initial': 'fade',
        'states': [{'name': 'fade', 'clip': 0, 'loop': False}], 'transitions': []}))
    project = root / 'test.azureproject'
    project.write_text(json.dumps({'schemaVersion': 1, 'id': 'node-opacity',
        'name': 'Node opacity contract', 'mounts': [{'name': 'assets', 'path': 'assets'}],
        'startupScene': 'assets:/scene.azurelevel'}))
    records = []

    def capture(name, opacity=1, mode='opaque', fixed=False, instances=False,
                overlap=None, outlined=True):
        model = copy.deepcopy(baseline)
        if mode == 'outline':
            model = copy.deepcopy(cube)
            model['materials'][0]['extras'] = {'azureRenderMaterial': {
                'schemaVersion': 2, 'class': 'skin',
                'features': ['silhouette-outline'] if outlined else []}}
        model['extensionsUsed'] = ['AZURE_node_opacity']
        model['nodes'][0]['extensions'] = {'AZURE_node_opacity': {
            'schemaVersion': 1, 'opacity': opacity}}
        if mode == 'blend':
            model['materials'][0]['alphaMode'] = 'BLEND'
        if instances:
            model = animated(model)
        (assets / 'sign.gltf').write_text(json.dumps(model))
        nodes = [{'id': 'sign', 'resourceId': 'sign'}]
        if instances:
            nodes = [{'id': 'sign-' + str(i), 'resourceId': 'sign', 'components': {
                'azure.transform': component('azure.transform', {'translation': [(i-1)*1.15, 0, 0]}),
                'azure.animator': component('azure.animator', {'asset': 'assets:/motion.json',
                    'state': 'fade', 'startTime': age}, 2)}}
                for i, age in enumerate((0, 1, 2))]
        resources = [{'id': 'sign', 'asset': 'assets:/sign.gltf'}]
        if overlap:
            back = copy.deepcopy(model)
            back['materials'][0]['pbrMetallicRoughness']['baseColorFactor'] = [.04, .12, .9, 1]
            (assets/'back.gltf').write_text(json.dumps(back))
            resources.append({'id': 'back', 'asset': 'assets:/back.gltf'})
            front_node = {'id': 'front', 'resourceId': 'sign', 'components': {
                'azure.transform': component('azure.transform', {
                    'translation': [-.26, 0, .35], 'scale': [.8, .8, .8]})}}
            back_node = {'id': 'back', 'resourceId': 'back', 'components': {
                'azure.transform': component('azure.transform', {
                    'translation': [.25, 0, -.35], 'scale': [1.3, 1.3, 1.3]})}}
            nodes = ([front_node] if overlap == 'front' else
                     [back_node] if overlap == 'back' else [front_node, back_node])
        if mode == 'depth':
            resources.append({'id': 'tint', 'asset': 'assets:/tint.gltf'})
            nodes.append({'id': 'tint', 'resourceId': 'tint', 'components': {
                'azure.transform': component('azure.transform', {'translation': [0, 0, .1], 'scale': [3, 3, 3]})}})
        if mode == 'shadow':
            resources.append({'id': 'receiver', 'asset': 'assets:/receiver.gltf'})
            nodes.append({'id': 'receiver', 'resourceId': 'receiver', 'components': {
                'azure.transform': component('azure.transform', {'translation': [0, 0, -.6], 'scale': [3, 3, 3]})}})
        nodes.append({'id': 'camera', 'components': {
            'azure.transform': component('azure.transform', {'translation': [0, 0, 3]}),
            'azure.camera': component('azure.camera', {'enabled': True, 'priority': 1, 'fieldOfView': 55})}})
        level = {'schemaVersion': 1, 'id': name, 'sceneType': 'character',
            'resources': resources, 'nodes': nodes, 'renderSettings': {
                'platform': False, 'silhouetteOutline': mode == 'outline' and outlined, 'innerOutline': False,
                'stylized': False, 'bloomEnabled': False, 'antiAliasing': 0,
                'skyMode': 2, 'skyColor': [.06, .12, .22], 'exposure': 0,
                'toneMapping': False, 'contrast': 1, 'saturation': 1}}
        (assets / 'scene.azurelevel').write_text(json.dumps(level))
        output = root / name
        output.mkdir()
        command = [str(player.resolve()), '--project', str(project.resolve()),
            '--width', '256', '--height', '192', '--capture-dir', str((output/'frames').resolve()),
            '--capture-frames', '1', '--fixed-frame-step', '--qa-camera', 'scene',
            '--qa-light', 'stylized-key' if mode == 'shadow' else 'scene', '--qa-isolation',
            'shadow-visibility' if mode == 'shadow' else 'albedo',
            '--runtime-report', str((output/'runtime.json').resolve())]
        if fixed:
            command += ['--disable-bindless', '--disable-compute-skinning', '--disable-parallel-recording']
        (output/'command.json').write_text(json.dumps(command, indent=2))
        result = subprocess.run(command, capture_output=True, encoding='utf-8', errors='replace', timeout=120)
        log = result.stdout + result.stderr
        (output/'player.log').write_text(log, encoding='utf-8')
        assert result.returncode == 0 and not re.search(r'VUID-|Validation Error|\[validation warning\]|\[error\]', log), name + '\n' + log[-3000:]
        assert 'Allocator after unload: buffers=0 images=0' in log, name
        runtime = json.loads((output/'runtime.json').read_text())
        assert not runtime['scriptErrors'] and not runtime['presentationErrors'], name
        records.append({'name': name, 'validationEnabled': 'Validation layer: enabled' in log})
        return np.asarray(Image.open(output/'frames/frame_000000.png').convert('RGB'), dtype=np.int16)

    frames = {}
    metrics = {}
    for mode in modes:
        for value in (0, .5, 1):
            frames[mode, value] = capture(mode + '-' + str(value), value, mode)
        hidden, half, full = (frames[mode, value] for value in (0, .5, 1))
        visible = np.max(np.abs(full-hidden), axis=2) > 12
        assert visible.sum() > 100, (mode, 'fixture has no visible response')
        response = float(np.abs(half-hidden)[visible].mean() / np.abs(full-hidden)[visible].mean())
        metrics[mode+'HalfResponse'] = response
        if mode == 'blend':
            # Alpha blends in linear light; PNG values use the sRGB transfer.
            def linear(image):
                value=image.astype(np.float64)/255
                return np.where(value<=.04045,value/12.92,((value+.055)/1.055)**2.4)
            low,mid,high=(linear(image) for image in (hidden,half,full))
            linearResponse=float(np.abs(mid-low)[visible].mean()/np.abs(high-low)[visible].mean())
            metrics['blendLinearHalfResponse']=linearResponse
            assert .48 < linearResponse < .52, (mode, linearResponse)
        elif mode == 'shadow':
            assert .1 < response < .9, (mode, response)
        else:
            maximum = .85 if mode == 'outline' else .65
            assert .35 < response < maximum, (mode, response)
            # Main and sampled depth must cover the same surface locations.
            endpoint = np.minimum(np.max(np.abs(half-hidden),axis=2), np.max(np.abs(half-full),axis=2))
            mismatches = int(np.count_nonzero(endpoint > 3))
            metrics[mode+'EndpointMismatches'] = mismatches
            assert mismatches <= 8, (mode, mismatches)
    if 'outline' in modes:
        front_plain = capture('overlap-front-plain', mode='outline', overlap='front', outlined=False)
        front_full = capture('overlap-front-full', mode='outline', overlap='front')
        front_half = capture('overlap-front-half', .5, 'outline', overlap='front')
        back_full = capture('overlap-back-full', mode='outline', overlap='back')
        pair_full = capture('overlap-pair-full', mode='outline', overlap='pair')
        pair_half = capture('overlap-pair-half', .5, 'outline', overlap='pair')
        hidden = frames['outline', 0]
        front_rim = ((front_full.max(axis=2) < 65)
                     & (np.max(np.abs(front_plain-hidden), axis=2) <= 3))
        back_surface = back_full[:,:,2] > back_full[:,:,0] + 30
        overlap_rim = front_rim & back_surface
        assert overlap_rim.sum() > 40, ('overlap rim fixture', int(overlap_rim.sum()))
        full_rim = (pair_full.max(axis=2) < 65) & overlap_rim
        metrics['overlapFullRimPixels'] = int(full_rim.sum())
        metrics['overlapExpectedRimPixels'] = int(overlap_rim.sum())
        assert full_rim.sum() >= overlap_rim.sum() - 4, ('overlap full rim', metrics)
        expected_half = overlap_rim & (front_half.max(axis=2) < 65)
        assert expected_half.sum() > 15, ('overlap half fixture', int(expected_half.sum()))
        missing_half = expected_half & (pair_half.max(axis=2) >= 65)
        metrics['overlapHalfRimMissingPixels'] = int(missing_half.sum())
        assert missing_half.sum() <= 4, ('overlap mask ownership', metrics)
        pair_fixed = capture('overlap-pair-half-fixed', .5, 'outline', fixed=True, overlap='pair')
        metrics['outlineFallbackMaximumDifference'] = int(np.abs(pair_fixed-pair_half).max())
        assert metrics['outlineFallbackMaximumDifference'] <= 1, metrics
    if ('opaque', .5) not in frames:
        frames['opaque', .5] = capture('opaque-half-reference', .5)
    fixed = capture('opaque-half-fixed', .5, fixed=True)
    metrics['fallbackMaximumDifference'] = int(np.abs(fixed-frames['opaque',.5]).max())
    assert metrics['fallbackMaximumDifference'] <= 1, metrics
    independent = capture('independent-ages', instances=True)
    # The fixed scene camera maps three equally spaced panes to these thirds.
    red = independent[:,:,0] > independent[:,:,2] + 30
    counts = [int(part.sum()) for part in np.array_split(red,3,axis=1)]
    metrics['independentVisiblePixels'] = counts
    assert counts[0] > 500 and .35 < counts[1]/counts[0] < .65 and counts[2] == 0, counts
    summary = {'modes': list(modes), 'metrics': metrics, 'captures': records, 'validationErrors': 0}
    (root/'summary.json').write_text(json.dumps(summary,indent=2))
    print(json.dumps(summary))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--modes', nargs='+', choices=['opaque','blend','depth','shadow','outline'],
        default=['opaque','blend','depth','shadow','outline'])
    args = parser.parse_args()
    if args.output:
        run(args.executable, args.output.resolve(), args.modes)
    else:
        with tempfile.TemporaryDirectory(prefix='azure_node_opacity_') as directory:
            run(args.executable, Path(directory)/'evidence', args.modes)
