"""Validate ordinary glTF magnification, wrapping and per-binding samplers."""
import argparse
import base64
import copy
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

import numpy as np
from PIL import Image
from create_visibility_fixture import component, surface
from test_unlit_texture_layers_gpu import linear


def run(player, root):
    root.mkdir(parents=True, exist_ok=False)
    inputs = [player.resolve(), Path(__file__).resolve(), *sorted((player.parent/'shaders').glob('*.spv'))]
    fingerprints = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    (root/'input-fingerprints.json').write_text(json.dumps(fingerprints, indent=2))
    assets = root / 'assets'
    assets.mkdir()
    surface(assets / 'template.gltf', double_sided=True)
    template = json.loads((assets / 'template.gltf').read_text())
    pixels = np.array([[[220, 40, 70, 255], [30, 200, 80, 255]],
                       [[60, 90, 230, 255], [180, 160, 40, 255]]], dtype=np.uint8)
    values = linear(pixels[:, :, :3])
    Image.fromarray(pixels).save(assets / 'pattern.png')
    (assets / 'pattern.png.azmeta').write_text(json.dumps(dict(schemaVersion=1,
        id='f75f1f52-6476-4b1f-86b8-46592bad1486', settings=dict(texture=dict(anisotropy=1)), dependencies=[])))
    records = []
    for wrap, coordinate in [(10497, .375), (33071, 1.375), (33648, 1.375)]:
        for fixed in [False, True]:
            name = f'wrap-{wrap}-' + ('fixed' if fixed else 'indexed')
            model = copy.deepcopy(template)
            data = base64.b64decode(model['buffers'][0]['uri'].split(',')[1])
            data += struct.pack('<8f', *([coordinate, .375] * 4))
            model['buffers'][0] = dict(byteLength=len(data), uri='data:application/octet-stream;base64,' + base64.b64encode(data).decode())
            model['bufferViews'].append(dict(buffer=0, byteOffset=len(data)-32, byteLength=32))
            model['accessors'].append(dict(bufferView=3, componentType=5126, count=4, type='VEC2'))
            model['meshes'][0]['primitives'][0]['attributes']['TEXCOORD_0'] = 3
            model['images'] = [dict(uri='pattern.png')]
            model['samplers'] = [dict(minFilter=9985, magFilter=9728, wrapS=wrap, wrapT=10497),
                                 dict(minFilter=9984, magFilter=9729, wrapS=wrap, wrapT=10497)]
            model['textures'] = [dict(source=0, sampler=i) for i in range(2)]
            model['materials'] = [dict(name=f'OrdinarySampler{i}', doubleSided=True,
                pbrMetallicRoughness=dict(baseColorFactor=[1, 1, 1, 1], baseColorTexture=dict(index=i), metallicFactor=0, roughnessFactor=1),
                extras=dict(azureRenderMaterial=dict(schemaVersion=2, **{'class': 'generic'}, features=['unlit-surface'],
                                                     surface=dict(schemaVersion=1, lighting='unlit')))) for i in range(2)]
            model['meshes'].append(copy.deepcopy(model['meshes'][0]))
            model['meshes'][1]['primitives'][0]['material'] = 1
            model['nodes'] = [dict(mesh=0, translation=[-.55, 0, 0]), dict(mesh=1, translation=[.55, 0, 0])]
            model['scenes'][0]['nodes'] = [0, 1]
            (assets / 'surface.gltf').write_text(json.dumps(model))
            camera = component('azure.camera', dict(enabled=True, priority=1, fieldOfView=55, nearPlane=.1, farPlane=20,
                                                    orthographic=True, orthographicHeight=2))
            camera['version'] = 2
            settings = dict(schemaVersion=20, sceneLighting=True, skyIntensity=0, platform=False, background=False,
                            silhouetteOutline=False, innerOutline=False, stylized=False, bloomEnabled=False,
                            antiAliasing=0, exposure=0, toneMapping=False, contrast=1, saturation=1)
            scene = dict(schemaVersion=1, id=name, sceneType='character', resources=[dict(id='surface', asset='assets:/surface.gltf')],
                         nodes=[dict(id='surface', resourceId='surface', components={}),
                                dict(id='camera', components={'azure.transform': component('azure.transform', dict(translation=[0, 0, 3])),
                                                             'azure.camera': camera})], lights=[], renderSettings=settings)
            (assets / 'scene.azurelevel').write_text(json.dumps(scene))
            (root / (name+'-input.json')).write_text(json.dumps(dict(model=model, scene=scene), indent=2))
            project = root / 'project.azureproject'
            project.write_text(json.dumps(dict(schemaVersion=1, id='gltf-sampler', name='glTF sampler',
                                              mounts=[dict(name='assets', path='assets')], startupScene='assets:/scene.azurelevel')))
            output = root / name
            command = [str(player.resolve()), '--project', str(project.resolve()), '--width', '128', '--height', '128',
                       '--capture-dir', str((output / 'frames').resolve()), '--capture-frames', '1', '--smoke-frames', '1',
                       '--fixed-frame-step', '--qa-camera', 'scene', '--qa-light', 'scene']
            if fixed:
                command += ['--disable-bindless', '--disable-compute-skinning', '--disable-parallel-recording']
            with (root / (name + '.log')).open('w') as log:
                subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=120)
            log = (root / (name + '.log')).read_text(errors='replace')
            assert 'Allocator after unload: buffers=0 images=0' in log
            assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]', log)
            image = np.asarray(Image.open(output / 'frames/frame_000000.png').convert('RGB'))
            x = .375 if wrap == 10497 else 1 if wrap == 33071 else .625
            nearest = values[0, 0 if x < .5 else 1]
            tx = np.clip(x*2-.5, 0, 1)
            bilinear = ((1-tx)*values[0, 0]+tx*values[0, 1])*.75 + ((1-tx)*values[1, 0]+tx*values[1, 1])*.25
            samples = []
            for column, expected in [(29, nearest), (99, bilinear)]:
                actual = linear(image[64, column])
                error = float(np.max(np.abs(actual-expected)))
                assert error < .008, (name, column, actual, expected, error)
                samples.append(dict(x=column, y=64, actual=actual.tolist(), expected=expected.tolist(), maximumLinearError=error))
            records.append(dict(case=name, samples=samples))
    result = dict(status='passed', budget=.008, cases=records,
                  maximumLinearError=max(p['maximumLinearError'] for row in records for p in row['samples']))
    (root / 'results.json').write_text(json.dumps(result, indent=2))
    assert all(hashlib.sha256(p.read_bytes()).hexdigest() == fingerprints[str(p)] for p in inputs)
    print(json.dumps(result))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--player', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    run(args.player, args.output)
