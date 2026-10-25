"""Validate node-owned NPR rim origins through production material and pose paths."""
import argparse
import base64
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import uuid

import numpy as np
from PIL import Image
from convert_material_semantics import complete_material_document
from create_visibility_fixture import component
from native_render.light_lab import generate


def unit(value):
    value = np.asarray(value, dtype=float)
    return value / np.linalg.norm(value)


def linear(value):
    value = np.asarray(value, dtype=float) / 255
    return np.where(value <= .04045, value / 12.92, ((value + .055) / 1.055) ** 2.4)


def reference(pixel, origin_x, translation, scale, light):
    # Orthographic camera at (0,0,5), height 2; authored tangent and neutral map.
    position = np.array([(pixel[0] + .5) / 128 - 1, 1 - (pixel[1] + .5) / 128, 0])
    authored = unit([.98, 0, .2])
    normal = unit(authored / scale)
    tangent = unit(np.array([authored[2], 0, -authored[0]]) * scale)
    tangent = unit(tangent - normal * np.dot(normal, tangent))
    bitangent = unit(np.cross(normal, tangent)) * np.sign(np.prod(scale))
    normal = unit(normal + (tangent + bitangent) / 255)
    view = unit(np.array([0, 0, 5]) - position)
    incoming = np.asarray(light) - position
    direction = unit(incoming)
    planar = unit([direction[0], direction[1], 0])
    edge = 1 - abs(np.dot(normal, view))
    band = np.clip((edge - .62) / .16, 0, 1)
    band = band * band * (3 - 2 * band)
    origin = np.asarray(translation) + np.asarray(scale) * [origin_x, 0, 0]
    radial = position - origin
    radial[1] = 1 / 16384
    visibility = np.clip(np.dot(unit(radial), direction) + 1, 0, 1)
    attenuation = (1 - min(np.dot(incoming, incoming) / 100, 1)) ** 2
    return np.full(3, .5 * band * visibility * max(np.dot(normal, planar), 0) * .1 * attenuation * 3)


def prepare(folder, name):
    generate(folder, 'point')
    model = json.loads((folder / 'plane.gltf').read_text())
    payload = bytearray(base64.b64decode(model['buffers'][0]['uri'].split(',', 1)[1]))
    normal = unit([.98, 0, .2])
    payload[48:96] = struct.pack('<12f', *(normal.tolist() * 4))
    view = len(model['bufferViews'])
    model['bufferViews'].append(dict(buffer=0, byteOffset=len(payload), byteLength=64))
    payload.extend(struct.pack('<16f', *([normal[2], 0, -normal[0], 1] * 4)))
    tangent = len(model['accessors'])
    model['accessors'].append(dict(bufferView=view, componentType=5126, count=4, type='VEC4'))
    model['meshes'][0]['primitives'][0]['attributes']['TANGENT'] = tangent
    x = .25 if name == 'right' else -.25
    model['nodes'].append(dict(name='Origin', translation=[x, 0, 0]))
    model['scenes'][0]['nodes'].append(1)
    if name == 'animated':
        accessors = []
        for values, kind, count in [([0, 1], 'SCALAR', 2), ([-.25, 0, 0, .25, 0, 0], 'VEC3', 2)]:
            view = len(model['bufferViews'])
            raw = struct.pack('<' + 'f' * len(values), *values)
            model['bufferViews'].append(dict(buffer=0, byteOffset=len(payload), byteLength=len(raw)))
            payload.extend(raw)
            accessors.append(len(model['accessors']))
            model['accessors'].append(dict(bufferView=view, componentType=5126, count=count, type=kind))
        model['animations'] = [dict(name='origin-motion', samplers=[dict(input=accessors[0], output=accessors[1], interpolation='LINEAR')],
                                   channels=[dict(sampler=0, target=dict(node=1, path='translation'))])]
    model['buffers'][0] = dict(byteLength=len(payload), uri='data:application/octet-stream;base64,' + base64.b64encode(payload).decode())
    Image.new('RGBA', (4, 2), (255, 255, 255, 255)).save(folder / 'lut.png')
    model['images'] = [dict(uri='lut.png')]
    model['textures'] = [dict(source=0)]
    material = model['materials'][0]
    material['pbrMetallicRoughness']['baseColorFactor'] = [1, 1, 1, 1]
    profile = dict(schemaVersion=2, **{'class': 'generic'}, features=[], shadowColorLut=dict(schemaVersion=1, texture=0, flipV=False),
                   nprDiffuse=dict(schemaVersion=1, shadowScale=1, lightingInputs='host-scene-radiance', shadowMask='none', baseAo='one',
                                   rimResponse='view-facing', rimOrigin=dict(schemaVersion=1, node='Origin')))
    if name == 'omitted':
        profile['nprDiffuse'].pop('rimOrigin')
    material['extras'] = dict(azureSlotId='surface', azureRenderMaterial=profile)
    (folder / 'plane.gltf').write_text(json.dumps(model))
    level = json.loads((folder / 'level.azurelevel').read_text())
    level['nodes'][1]['components']['azure.camera']['data']['orthographicHeight'] = 2
    scale, translation, light = [1, 1, 1], [0, 0, 0], [2, 0, 1]
    if name == 'transformed':
        scale, translation = [1.4, .6, 1.2], [.2, .1, 0]
    elif name == 'mirrored':
        scale, translation, light = [-1.2, .7, 1], [-.2, .1, 0], [-2, 0, 1]
    level['nodes'][0]['components'] = {'azure.transform': component('azure.transform', dict(translation=translation, scale=scale))}
    level['nodes'][2]['components']['azure.transform']['data']['translation'] = light
    level['lights'][0].update(color=[1, 1, 1], intensity=.1, radius=10,
        shape=dict(type='point', falloff='range-power', falloffExponent=2,
                   characterResponse=dict(schemaVersion=5, composition='material-rim', width=.3, surfaceTint=0)))
    level['renderSettings'].update(exposure=0, contrast=1, saturation=1, nprLighting=dict(schemaVersion=1, enabled=True))
    if name == 'document':
        image_id, document_id = str(uuid.uuid4()), str(uuid.uuid4())
        (folder / 'lut.png.azmeta').write_text(json.dumps(dict(schemaVersion=1, id=image_id, settings={}, dependencies=[])))
        (folder / 'surface.azurematerial').write_text(json.dumps(complete_material_document(material, model, {'0': image_id})))
        (folder / 'surface.azurematerial.azmeta').write_text(json.dumps(dict(schemaVersion=1, id=document_id, settings={}, dependencies=[image_id])))
        level['nodes'][0]['materialSlots'] = {'surface': document_id}
    (folder / 'level.azurelevel').write_text(json.dumps(level))
    pixel = [round((translation[0] + 1) * 128 - .5), round((1 - translation[1]) * 128 - .5)]
    return pixel, x, translation, scale, light


def run(player, output, expected_validation=None):
    output.mkdir(parents=True, exist_ok=False)
    summary = dict(schemaVersion=1, executableSha256=hashlib.sha256(player.read_bytes()).hexdigest(), linearErrorBudget=.008, captures=[])
    images = {}
    for name in ['left', 'left-fixed', 'right', 'omitted', 'transformed', 'mirrored', 'document', 'animated']:
        folder = output / name
        pixel, x, translation, scale, light = prepare(folder, name)
        count = 32 if name == 'animated' else 1
        command = [str(player.resolve()), '--project', str(folder / 'lab.azureproject'), '--width', '256', '--height', '256',
                   '--capture-dir', str(folder / 'frames'), '--capture-frames', str(count), '--capture-fps', '30', '--fixed-frame-step',
                   '--qa-camera', 'scene', '--qa-light', 'scene', '--qa-isolation', 'rim', '--runtime-report', str(folder / 'runtime.json')]
        if name == 'left-fixed':
            command += ['--disable-bindless', '--disable-compute-skinning', '--disable-parallel-recording']
        if name == 'animated':
            command += ['--qa-animation']
        (folder / 'command.json').write_text(json.dumps(command, indent=2))
        print(name, flush=True)
        with (folder / 'player.log').open('w') as log:
            subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=180)
        log = (folder / 'player.log').read_text(encoding='utf-8', errors='replace')
        assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]', log), name
        assert 'Allocator after unload: buffers=0 images=0' in log, name
        validation = 'Validation layer: enabled' in log
        if expected_validation is not None:
            assert validation == (expected_validation == 'enabled'), name
        runtime = json.loads((folder / 'runtime.json').read_text())
        assert not runtime['scriptErrors'] and not runtime['presentationErrors'], name
        frames = sorted((folder / 'frames').glob('frame_*.png'))
        assert len(frames) == count, name
        values = [linear(np.asarray(Image.open(path).convert('RGB'))[pixel[1], pixel[0]]) for path in frames]
        expected = reference(pixel, 0 if name == 'omitted' else x, translation, scale, light)
        error = float(np.max(np.abs(values[0] - expected)))
        row = dict(name=name, validationEnabled=validation, frames=count, pixel=pixel, expected=expected.tolist(),
                   measured=values[0].tolist(), maximumLinearError=error)
        if name == 'animated':
            row['temporalRange'] = float(np.ptp(np.asarray(values), axis=0).max())
        else:
            images[name] = np.asarray(Image.open(frames[0]).convert('RGB'))
        summary['captures'].append(row)
        (output / 'summary.json').write_text(json.dumps(summary, indent=2))
        assert error <= .008, (name, row)
        if name == 'animated':
            assert row['temporalRange'] > .05, row
    assert np.array_equal(images['left'], images['left-fixed']), 'Descriptor and skinning paths differ'
    assert np.array_equal(images['left'], images['document']), 'Complete material document differs from glTF'
    assert np.max(np.abs(linear(images['left'][128, 128]) - linear(images['right'][128, 128]))) > .05
    summary['status'] = 'passed'
    (output / 'summary.json').write_text(json.dumps(summary, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--expected-validation', choices=['enabled', 'disabled'])
    args = parser.parse_args()
    if args.output:
        run(args.executable, args.output.resolve(), args.expected_validation)
    else:
        with tempfile.TemporaryDirectory(prefix='azure-material-origin-') as temporary:
            run(args.executable, Path(temporary) / 'capture', args.expected_validation)
