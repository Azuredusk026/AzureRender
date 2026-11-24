"""Create self-contained CC0 surfaces for winding, depth and clip tests."""
import base64
import json
from pathlib import Path
import struct
import io
from PIL import Image


def surface(path, *, baked_mirror=False, baked_scale=None, double_sided=False, alpha=False, sloped=False, normal_map=False):
    # Counterclockwise triangles point toward +Z, independently of the engine.
    positions = [(-.5, -.5, 0), (.5, -.5, 0), (.5, .5, 0), (-.5, .5, 0)]
    if sloped: positions = [(x, y, -x) for x, y, z in positions]
    values = [v for p in positions for v in p] + [v for _ in positions for v in ((.70710678, 0, .70710678) if sloped else (0, 0, 1))]
    binary = struct.pack('<24f6H', *values, 0, 1, 2, 0, 2, 3)
    material = {'doubleSided': double_sided,
                'pbrMetallicRoughness': {'baseColorFactor': [.9, .15, .08, .5 if alpha else 1], 'metallicFactor': 0, 'roughnessFactor': 1}}
    if alpha: material['alphaMode'] = 'BLEND'
    document = {'asset': {'version': '2.0', 'generator': 'Azure visibility fixture', 'extras': {'license': 'CC0-1.0'}},
        'scene': 0, 'scenes': [{'nodes': [0]}], 'nodes': [{'mesh': 0, 'scale': baked_scale or ([-1, 1, 1] if baked_mirror else [1, 1, 1])}],
        'buffers': [{'byteLength': len(binary), 'uri': 'data:application/octet-stream;base64,' + base64.b64encode(binary).decode()}],
        'bufferViews': [{'buffer': 0, 'byteOffset': 0, 'byteLength': 48}, {'buffer': 0, 'byteOffset': 48, 'byteLength': 48},
                        {'buffer': 0, 'byteOffset': 96, 'byteLength': 12}],
        'accessors': [{'bufferView': 0, 'componentType': 5126, 'count': 4, 'type': 'VEC3',
                      'min': [-.5, -.5, -.5 if sloped else 0], 'max': [.5, .5, .5 if sloped else 0]},
                      {'bufferView': 1, 'componentType': 5126, 'count': 4, 'type': 'VEC3'},
                      {'bufferView': 2, 'componentType': 5123, 'count': 6, 'type': 'SCALAR'}],
        'materials': [material], 'meshes': [{'primitives': [{'attributes': {'POSITION': 0, 'NORMAL': 1}, 'indices': 2, 'material': 0, 'mode': 4}]}]}
    if normal_map:
        tangent = (.70710678, 0, -.70710678, 1) if sloped else (1, 0, 0, 1)
        binary += b'\0\0\0\0' + struct.pack('<16f', *(tangent*4))
        document['buffers'][0] = {'byteLength': len(binary), 'uri': 'data:application/octet-stream;base64,' + base64.b64encode(binary).decode()}
        document['bufferViews'].append({'buffer': 0, 'byteOffset': 112, 'byteLength': 64})
        document['accessors'].append({'bufferView': 3, 'componentType': 5126, 'count': 4, 'type': 'VEC4'})
        document['meshes'][0]['primitives'][0]['attributes']['TANGENT'] = 3
        pixel = io.BytesIO(); Image.new('RGB', (1, 1), (128, 218, 218)).save(pixel, format='PNG')
        document['images'] = [{'uri': 'data:image/png;base64,' + base64.b64encode(pixel.getvalue()).decode()}]
        document['textures'] = [{'source': 0}]
        material['normalTexture'] = {'index': 0}
    path.write_text(json.dumps(document), encoding='utf-8')


def component(kind, values):
    return {'type': kind, 'version': 1, 'data': values}


def create(root, *, scale=(1, 1, 1), rotation=(0, 0, 0), baked=False, double=False,
           far=False, camera_far=500, outline=True, depth=False, alpha=False, sloped=False, baked_scale=None, normal_map=False,
           offset=None, mixed=False):
    root = Path(root)
    assets = root / 'assets'
    assets.mkdir(parents=True, exist_ok=True)
    surface(assets / 'surface.gltf', baked_mirror=baked, double_sided=double, alpha=alpha, sloped=sloped, baked_scale=baked_scale, normal_map=normal_map)
    nodes = [{'id': 'surface', 'resourceId': 'surface', 'components': {
        'azure.transform': component('azure.transform', {'translation': offset or ([0, -44, -150] if far else [0, 0, 0]),
            'rotation': rotation, 'scale': [15, 15, 15] if far else scale})}}]
    resources = [{'id': 'surface', 'asset': 'assets:/surface.gltf'}]
    if far or depth or sloped or offset or mixed:
        # The invisible first resource keeps the QA lens at its fixed origin.
        surface(assets / 'anchor.gltf')
        resources.insert(0, {'id': 'anchor', 'asset': 'assets:/anchor.gltf'})
    if mixed:
        nodes[0]['components']['azure.transform']['data'].update(translation=[-.7, 0, 0], scale=[.5, .5, .5])
        nodes.append({'id': 'mirror', 'resourceId': 'surface', 'components': {
            'azure.transform': component('azure.transform', {'translation': [.7, 0, 0], 'scale': [-.5, .5, .5]})}})
    if depth:
        surface(assets / 'front.gltf')
        data = json.loads((assets / 'front.gltf').read_text())
        data['materials'][0]['pbrMetallicRoughness']['baseColorFactor'] = [.05, .8, .1, 1]
        (assets / 'front.gltf').write_text(json.dumps(data), encoding='utf-8')
        resources.insert(1, {'id': 'front', 'asset': 'assets:/front.gltf'})
        # Near green occluder is submitted first; red rear must fail depth.
        nodes.insert(0, {'id': 'front', 'resourceId': 'front', 'components': {
            'azure.transform': component('azure.transform', {'translation': [0, 0, .1]})}})
    settings = {'platform': False, 'cameraNear': .1, 'cameraFar': camera_far, 'shadowDistance': 100,
                'silhouetteOutline': outline, 'innerOutline': outline}
    (assets / 'scene.azurelevel').write_text(json.dumps({'schemaVersion': 1, 'id': 'visibility', 'sceneType': 'character',
        'resources': resources, 'nodes': nodes, 'renderSettings': settings}), encoding='utf-8')
    project = root / 'project.azureproject'
    project.write_text(json.dumps({'schemaVersion': 1, 'id': 'visibility', 'name': 'Visibility fixture',
        'mounts': [{'name': 'assets', 'path': 'assets'}], 'startupScene': 'assets:/scene.azurelevel'}), encoding='utf-8')
    return project
