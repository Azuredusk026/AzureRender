"""Verify projected hair through Player scene lights and exact pixel rays.

Preparation writes twelve self-contained cases and CPU expectations. GPU
execution is explicit; preparation never launches Player. The fixed .008
linear RGB budget includes 8-bit sRGB output quantization. This verifies the
host contract, not the uncaptured native main-light virtual-direction globals.
"""
import argparse
import base64
import copy
import hashlib
from io import BytesIO
import json
import math
from pathlib import Path
import re
import struct
import subprocess
import tempfile

import numpy as np
from PIL import Image

from create_visibility_fixture import component
from endfield_native.projected_hair import project_coordinates, evaluate_energy, virtual_half_vector


SIZE = 128
FOV = 55
CAMERA = np.array([0., 0., 3.])
BUDGET = .008
LIGHT_INTENSITY = .5
CONTROL = (137, 188, 225, 255)
RAMP_ROWS = ((255, 16, 0), (192, 64, 8), (128, 128, 16), (64, 192, 32),
             (16, 255, 64), (8, 192, 128), (16, 128, 192), (32, 64, 255))
RAMP = np.asarray([[tuple(round(c * gain) for c in row) + (255,)
                    for gain in (.4, .6, .8, 1)] for row in RAMP_ROWS], dtype=np.uint8)


def linear(value):
    value = np.asarray(value, dtype=float) / 255
    return np.where(value <= .04045, value / 12.92, ((value + .055) / 1.055) ** 2.4)


def normalized(value):
    value = np.asarray(value, dtype=float)
    return value / np.linalg.norm(value)


def rotation(degrees):
    angle = math.radians(degrees)
    c, s = math.cos(angle), math.sin(angle)
    return np.array([[c, -s, 0], [s, c, 0], [0, 0, 1.]])


def cases():
    default = {'mirror': False, 'rotation': 0, 'reverse': False, 'local': False,
        'dark': False, 'disabled': False, 'fixed': False, 'control': False,
        'left': {'r': 0, 'g': 128, 'a': 0, 'normal': (128, 128, 128, 128),
                 'surfaceScale': 0, 'specularScale': 0},
        'right': {'r': 255, 'g': 128, 'a': 0, 'normal': (128, 128, 128, 128),
                  'surfaceScale': 0, 'specularScale': 0}}
    result = []
    for name in ('baseline', 'reverse-light', 'zero-parameters', 'secondary-directional',
                 'secondary-local', 'split-normals', 'mirrored', 'rotated',
                 'dark', 'disabled', 'fixed', 'linear-control', 'frame-identity', 'frame-turned', 'frame-turned-fixed',
                 'virtual-main','virtual-fixed','virtual-zero-gain','virtual-point','virtual-control'):
        case = copy.deepcopy(default)
        case['name'] = name
        if name == 'reverse-light': case['reverse'] = True
        if name == 'zero-parameters':
            for side in ('left', 'right'): case[side].update(g=0, a=0)
        if name.startswith('secondary-'):
            case['left'].update(g=0, a=128)
            case['local'] = name == 'secondary-local'
        if name == 'split-normals':
            case['left'].update(normal=(216, 70, 128, 128), surfaceScale=1)
            case['right'].update(normal=(128, 128, 192, 128), specularScale=1)
        if name == 'mirrored': case['mirror'] = True
        if name == 'rotated': case['rotation'] = 90
        for flag in ('dark', 'disabled', 'fixed'):
            if name == flag: case[flag] = True
        if name == 'linear-control': case['control'] = True
        if name.startswith('frame-'):
            case['materialFrame']=90 if 'turned' in name else 0
            if name.endswith('-fixed'):case['fixed']=True
        if name.startswith('virtual-'):
            case['virtual']=name!='virtual-control';case['objectGain']=0 if name=='virtual-zero-gain' else 1
            case['fixed']=name=='virtual-fixed';case['local']=name=='virtual-point'
            case['lightY']=.6;case['primaryShift']=-1/3
        result.append(case)
    return result


def samples(case):
    # Pixel centres map to fixed points strictly inside each panel. A rigid
    # image-space rotation/reflection follows the corresponding model change.
    result = []
    for side, start in (('left', 44), ('right', 80)):
        for y in range(62, 66):
            for x in range(start, start + 4):
                px, py = x, y
                if case['mirror']: px = SIZE - 1 - px
                if case['rotation'] == 90: px, py = py, SIZE - 1 - px
                result.append((side, px, py))
    return result


def ramp_sample(uv):
    height, width = RAMP.shape[:2]
    pixel = np.asarray(uv) * (width, height) - .5
    first = np.floor(pixel).astype(int)
    fraction = pixel - first
    color = np.zeros(3)
    rows = set()
    for dy in range(2):
        for dx in range(2):
            x = min(max(first[0] + dx, 0), width - 1)
            y = min(max(first[1] + dy, 0), height - 1)
            weight = (fraction[0] if dx else 1 - fraction[0]) * (fraction[1] if dy else 1 - fraction[1])
            color += RAMP[y, x, :3] / 255 * weight
            if weight > 1e-10: rows.add(int(y))
    return color, sorted(rows)


def expectation(case):
    result = []
    turn = rotation(case['rotation'])
    mirror = np.diag([-1., 1., 1.]) if case['mirror'] else np.eye(3)
    frame = turn @ mirror
    object_frame=frame @ rotation(case.get('materialFrame',0))
    tangent = (*tuple(frame[:, 0]), -1 if case['mirror'] else 1)
    bitangent = np.cross((0, 0, 1), tangent[:3]) * tangent[3]
    light_y=case.get('lightY',.04)
    light = turn @ np.array([0., -light_y if case['reverse'] else light_y, math.sqrt(1 - light_y**2)])
    point_light = turn @ np.array([0., .16, 4.])
    projection_scale = math.tan(math.radians(FOV) * .5) * CAMERA[2]
    for side, x, y in samples(case):
        point = np.array([((x + .5) / SIZE * 2 - 1) * projection_scale,
                          (1 - (y + .5) / SIZE * 2) * projection_scale, 0])
        local_point = np.linalg.inv(frame) @ point
        assert (-.9 < local_point[0] < 0) if side == 'left' else (0 < local_point[0] < .9)
        assert abs(local_point[1]) < .9
        source = case[side]
        view = normalized(CAMERA - point)
        direction = normalized(point_light - point) if case['local'] else light
        xy = np.asarray(source['normal'][2:]) / 255 * 2 - 1
        z = math.sqrt(max(1 - min(float(xy @ xy), 1), 0))
        n = normalized(np.asarray(tangent[:3]) * xy[0] * source['specularScale']
                       + bitangent * xy[1] * source['specularScale'] + np.array([0, 0, z]))
        parameters = (source['r'] / 255, source['g'] / 255, 37 / 255, source['a'] / 255)
        half=virtual_half_vector(view=view,light=direction,object_x=object_frame[:,0],object_y=object_frame[:,1],object_z=object_frame[:,2],blend=1) if case.get('virtual') and not case['local'] else normalized(view+direction)
        coordinates = project_coordinates(half_vector=half,
            highlight_normal=n, view=view, source_parameters=parameters, tangent=tangent,
            object_x=object_frame[:, 0], object_y=object_frame[:, 1], object_z=object_frame[:, 2],
            primary_shift=case.get('primaryShift',0), secondary_shift=0, range2=1, edge_fade_power=2, direction_x=0)
        ramp, rows = ramp_sample(coordinates.uv)
        energy = evaluate_energy(coordinates, ramp, parameters, 1, (0, 0, .5))
        attenuation = 1.
        if case['local']:
            distance = float(np.linalg.norm(point_light - point))
            attenuation = max(1 - distance / 10, 0)**2 / (1 + distance * distance * .12)
        expected = (np.asarray(energy.primary) + (0 if case['local'] else np.asarray(energy.secondary)))
        expected *= LIGHT_INTENSITY * attenuation * 3  # hair-kk isolation multiplier
        expected *= case.get('objectGain',1)
        if case['dark'] or case['disabled']: expected[:] = 0
        if case['control']: expected = linear(CONTROL[:3])
        assert np.isfinite(expected).all() and (expected >= 0).all() and (expected < 1).all()
        result.append({'side': side, 'pixel': [x, y], 'worldPoint': point.tolist(),
            'uv': list(coordinates.uv), 'rampRows': rows, 'linearRgb': expected.tolist()})
    return result


def image_uri(image):
    stream = BytesIO()
    image.save(stream, format='PNG')
    return 'data:image/png;base64,' + base64.b64encode(stream.getvalue()).decode()


def model(case):
    binary = bytearray()
    views, accessors, primitives, materials, images, textures = [], [], [], [], [], []

    def accessor(values, components, kind, *, indices=False, bounds=False):
        while len(binary) % 4: binary.append(0)
        start = len(binary)
        flattened = np.asarray(values).reshape(-1).tolist()
        binary.extend(struct.pack('<' + ('H' if indices else 'f') * len(flattened), *flattened))
        views.append({'buffer': 0, 'byteOffset': start, 'byteLength': len(binary) - start})
        value = {'bufferView': len(views) - 1, 'componentType': 5123 if indices else 5126,
                 'count': len(flattened) // components, 'type': kind}
        if bounds:
            array = np.asarray(values)
            value.update(min=array.min(axis=0).tolist(), max=array.max(axis=0).tolist())
        accessors.append(value)
        return len(accessors) - 1

    def texture(image):
        images.append({'uri': image_uri(image)})
        textures.append({'source': len(images) - 1})
        return len(textures) - 1

    albedo = texture(Image.new('RGBA', (2, 2), CONTROL))
    ramp = texture(Image.fromarray(RAMP, 'RGBA'))
    for side, left, right in (('left', -.9, 0), ('right', 0, .9)):
        data = case[side]
        parameters = texture(Image.new('RGBA', (2, 2), (data['r'], data['g'], 37, data['a'])))
        normals = texture(Image.new('RGBA', (2, 2), data['normal']))
        materials.append({'name': side, 'doubleSided': True,
            'pbrMetallicRoughness': {'baseColorTexture': {'index': albedo},
                'metallicFactor': 0, 'roughnessFactor': 1},
            'extras': {'azureRenderMaterial': {'schemaVersion': 2, 'class': 'hair',
                'features': ['hair-anisotropy', 'hair-projected-ramp'], 'featureParameters': [0, 1, 0, 0],
                'hair': {'power': 200, 'strength': 4, 'rampRow': 3, 'strandShift': .1,
                    'upperLimit': 16, 'dataTexture': normals, 'dataNormalY': 'positive',
                    'projectedRamp': {'schemaVersion': 1, 'parameterTexture': parameters, 'rampTexture': ramp,
                        'primaryShift': case.get('primaryShift',0), 'secondaryShift': 0, 'intensity': 1, 'secondaryRange': 1,
                        'edgeFade': 2, 'specularNormalScale': data['specularScale'],
                        'surfaceNormalScale': data['surfaceScale'], 'directionX': 0,
                        'secondaryColor': [0, 0, .5], 'objectBasis': [1, 0, 0, 0, 1, 0, 0, 0, 1]}}}}})
        positions = [(left, -.9, 0), (right, -.9, 0), (right, .9, 0), (left, .9, 0)]
        attributes = {'POSITION': accessor(positions, 3, 'VEC3', bounds=True),
            'NORMAL': accessor([(0, 0, 1)] * 4, 3, 'VEC3'),
            'TANGENT': accessor([(1, 0, 0, 1)] * 4, 4, 'VEC4'),
            'TEXCOORD_0': accessor([(.5, .5)] * 4, 2, 'VEC2')}
        primitives.append({'attributes': attributes, 'indices': accessor([0, 1, 2, 0, 2, 3], 1, 'SCALAR', indices=True),
                           'material': len(materials) - 1})
    frame_nodes=[]
    if case.get('virtual'):
        for material in materials:material['extras']['azureRenderMaterial']['hair']['projectedRamp']['virtualLight']={
            'schemaVersion':1,'blend':1,'objectGain':case['objectGain']}
    if 'materialFrame' in case:
        angle=math.radians(case['materialFrame'])*.5
        frame_nodes=[{'name':'HairFrame','rotation':[0,0,math.sin(angle),math.cos(angle)]}]
        for material in materials:material['extras']['azureRenderMaterial']['hair']['projectedRamp']['frame']={
            'schemaVersion':1,'node':'HairFrame','basis':[1,0,0,0,1,0,0,0,1]}
    return {'asset': {'version': '2.0', 'generator': 'Azure projected hair contract fixture',
            'extras': {'license': 'CC0-1.0'}}, 'scene': 0, 'scenes': [{'nodes': [0]}],
        'nodes': [{'mesh': 0}]+frame_nodes, 'meshes': [{'primitives': primitives}], 'materials': materials,
        'images': images, 'textures': textures, 'bufferViews': views, 'accessors': accessors,
        'buffers': [{'byteLength': len(binary), 'uri': 'data:application/octet-stream;base64,'
                     + base64.b64encode(binary).decode()}]}


def prepare(case, root):
    assets = root / 'assets'
    assets.mkdir(parents=True)
    (assets / 'panels.gltf').write_text(json.dumps(model(case)), encoding='utf8')
    turn = rotation(case['rotation'])
    light_y=case.get('lightY',.04)
    direction = turn @ np.array([0, -light_y if case['reverse'] else light_y, math.sqrt(1 - light_y**2)])
    point = turn @ np.array([0, .16, 4.])
    level = {'schemaVersion': 1, 'id': case['name'], 'sceneType': 'character',
        'renderSettings': {'platform': False, 'background': False, 'silhouetteOutline': False,
            'innerOutline': False, 'stylized': False, 'sceneLighting': True,
            'skyIntensity': 0, 'bloomEnabled': False, 'antiAliasing': 0,
            'exposure': 0, 'toneMapping': False, 'contrast': 1, 'saturation': 1},
        'resources': [{'id': 'panels', 'asset': 'assets:/panels.gltf'}],
        'nodes': [{'id': 'panels', 'resourceId': 'panels', 'components': {
            'azure.transform': component('azure.transform', {'translation': [0, 0, 0],
                'rotation': [0, 0, case['rotation']], 'scale': [-1, 1, 1] if case['mirror'] else [1, 1, 1]})}},
            {'id': 'camera', 'components': {'azure.transform': component('azure.transform', {'translation': CAMERA.tolist()}),
                'azure.camera': component('azure.camera', {'enabled': True, 'priority': 1, 'fieldOfView': FOV})}},
            {'id': 'light', 'components': {'azure.transform': component('azure.transform', {'translation': point.tolist()})}}],
        'lights': [{'id': 'light', 'nodeId': 'light', 'color': [1, 1, 1],
            'intensity': LIGHT_INTENSITY, 'radius': 10, 'enabled': not case['dark'],
            'shape': {'type': 'point' if case['local'] else 'directional',
                'direction': (-direction).tolist(), 'falloff': 'legacy'}}]}
    (assets / 'scene.azurelevel').write_text(json.dumps(level), encoding='utf8')
    project = root / 'test.azureproject'
    project.write_text(json.dumps({'schemaVersion': 1, 'id': case['name'], 'name': 'Projected hair fixture',
        'mounts': [{'name': 'assets', 'path': 'assets'}], 'startupScene': 'assets:/scene.azurelevel'}), encoding='utf8')
    expected = expectation(case)
    (root / 'expected.json').write_text(json.dumps({'case': case, 'samples': expected,
        'outputEncoding': 'sRGB', 'maximumLinearError': BUDGET}, indent=2), encoding='utf8')
    return project, expected


def preflight(prepared):
    values = {name: np.asarray([sample['linearRgb'] for sample in data]) for name, data in prepared.items()}
    assert np.max(values['baseline']) > .02
    assert np.max(np.abs(values['baseline'] - values['reverse-light'])) > .02
    assert np.max(values['zero-parameters']) == 0
    assert np.max(values['secondary-directional'][:16, :2]) == 0
    assert np.min(values['secondary-directional'][:16, 2]) > .2
    assert np.max(values['secondary-local'][:16]) == 0
    assert np.max(values['secondary-local'][16:]) > .005  # positive local-light control
    assert np.max(np.abs(values['split-normals'][:16] - values['baseline'][:16])) == 0
    assert np.max(np.abs(values['split-normals'][16:] - values['baseline'][16:])) > .01
    for name in ('mirrored', 'rotated', 'fixed'):
        assert np.max(np.abs(values[name] - values['baseline'])) < 1e-12
    for name in ('dark', 'disabled'): assert np.max(values[name]) == 0
    assert np.max(np.abs(values['linear-control'] - linear(CONTROL[:3]))) == 0
    assert np.array_equal(values['frame-identity'],values['baseline'])
    assert np.max(np.abs(values['frame-turned']-values['baseline']))>.01
    assert np.array_equal(values['frame-turned'],values['frame-turned-fixed'])
    assert np.array_equal(values['virtual-main'],values['virtual-fixed'])
    assert np.max(np.abs(values['virtual-main']-values['virtual-control']))>.01
    assert np.max(values['virtual-zero-gain'])==0


def capture(player, case, project, root, expected, require_validation):
    command = [str(player.resolve()), '--project', str(project.resolve()), '--width', str(SIZE), '--height', str(SIZE),
        '--capture-dir', str((root / 'frames').resolve()), '--capture-frames', '1', '--fixed-frame-step',
        '--qa-camera', 'scene', '--qa-light', 'scene', '--qa-isolation', 'albedo' if case['control'] else 'hair-kk',
        '--runtime-report', str((root / 'runtime.json').resolve())]
    if case['fixed']: command += ['--disable-bindless', '--disable-compute-skinning', '--disable-parallel-recording']
    if case['disabled']: command += ['--qa-effect', 'hair-kk', '--qa-effect-state', 'disabled']
    (root / 'command.json').write_text(json.dumps(command, indent=2), encoding='utf8')
    with (root / 'player.log').open('w', encoding='utf8') as stream:
        subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT, check=True, timeout=120)
    log = (root / 'player.log').read_text(encoding='utf8', errors='replace')
    assert not re.search(r'VUID-|Validation Error|Validation Warning|\[error\]', log), case['name']
    assert 'Allocator after unload: buffers=0 images=0' in log, case['name']
    validation = 'Validation layer: enabled' in log
    assert validation or not require_validation, 'This batch requires the validation layer'
    runtime = json.loads((root / 'runtime.json').read_text())
    assert not runtime['scriptErrors'] and not runtime['presentationErrors'], case['name']
    frame = root / 'frames/frame_000000.png'
    pixels = np.asarray(Image.open(frame).convert('RGB'))
    assert pixels.shape == (SIZE, SIZE, 3), case['name']
    measured = np.asarray([linear(pixels[sample['pixel'][1], sample['pixel'][0]]) for sample in expected])
    wanted = np.asarray([sample['linearRgb'] for sample in expected])
    error = np.abs(measured - wanted)
    maximum = float(error.max())
    report = {'name': case['name'], 'validationEnabled': validation, 'allocatorAfterUnload': {'buffers': 0, 'images': 0},
        'image': str(frame.resolve()), 'imageSha256': hashlib.sha256(frame.read_bytes()).hexdigest(),
        'maximumLinearError': maximum, 'linearErrorBudget': BUDGET,
        'leftMeasuredMean': measured[:16].mean(axis=0).tolist(), 'rightMeasuredMean': measured[16:].mean(axis=0).tolist(),
        'sampledRampRows': sorted({row for sample in expected for row in sample['rampRows']})}
    (root / 'measurement.json').write_text(json.dumps(report, indent=2), encoding='utf8')
    assert maximum <= BUDGET, f"{case['name']}: exact pixel-ray error {maximum} exceeds {BUDGET}"
    if case['name'] in ('zero-parameters', 'dark', 'disabled'):
        assert np.max(measured) == 0, f"{case['name']}: manufactured direct highlight energy"
    if case['name'] == 'secondary-local':
        assert np.max(measured[:16]) == 0, 'Local lights must contribute only the primary lobe'
    return report, pixels


def run(player, output, *, prepare_only=False, require_validation=False):
    output.mkdir(parents=True, exist_ok=False)
    definitions = cases()
    prepared, projects = {}, {}
    for case in definitions:
        projects[case['name']], prepared[case['name']] = prepare(case, output / case['name'])
    preflight(prepared)
    summary = {'schemaVersion': 1, 'status': 'prepared', 'caseCount': len(definitions), 'captures': [],
        'maximumLinearError': BUDGET, 'outputEncoding': 'sRGB', 'cpuPreflightPassed': True,
        'rayContract': {'camera': CAMERA.tolist(), 'fieldOfView': FOV, 'planeZ': 0, 'size': [SIZE, SIZE]},
        'limitations': ['Main-light blend and object gain use explicit material inputs; captured shadow-field derivation is separate.',
            'This suite validates the explicit host projected-ramp contract and actual scene lights.',
            'Native SPECULAR_LINE and runtime object-frame equivalence require separate evidence.',
            'Prepared CPU expectations are not GPU execution evidence.']}
    report_path = output / 'summary.json'
    if not prepare_only:
        summary.update(player=str(player.resolve()), playerSha256=hashlib.sha256(player.read_bytes()).hexdigest())
        frames = {}
        try:
            for case in definitions:
                print(json.dumps({'capture': case['name'], 'status': 'starting'}), flush=True)
                record, pixels = capture(player, case, projects[case['name']], output / case['name'],
                                         prepared[case['name']], require_validation)
                summary['captures'].append(record)
                frames[case['name']] = pixels
            difference = int(np.abs(frames['fixed'].astype(int) - frames['baseline'].astype(int)).max())
            summary['fixedPathMaximumByteDifference'] = difference
            assert difference == 0, 'Fixed and bindless descriptor paths differ'
            summary['status'] = 'passed'
        except Exception as error:
            summary.update(status='failed', failure=str(error))
            raise
        finally:
            report_path.write_text(json.dumps(summary, indent=2), encoding='utf8')
    else:
        report_path.write_text(json.dumps(summary, indent=2), encoding='utf8')
    print(json.dumps({'status': summary['status'], 'caseCount': len(definitions), 'summary': str(report_path.resolve())}))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--prepare-only', action='store_true')
    parser.add_argument('--require-validation', action='store_true')
    args = parser.parse_args()
    if not args.prepare_only and args.executable is None:
        parser.error('--executable is required for GPU execution')
    if args.output:
        run(args.executable, args.output.resolve(), prepare_only=args.prepare_only,
            require_validation=args.require_validation)
    else:
        runs = Path(__file__).resolve().parents[1] / 'build/runs'
        runs.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='projected-hair-', dir=runs) as temporary:
            run(args.executable, Path(temporary) / 'capture', prepare_only=args.prepare_only,
                require_validation=args.require_validation)
