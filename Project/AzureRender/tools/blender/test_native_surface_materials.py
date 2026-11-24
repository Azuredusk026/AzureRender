"""Bake production surface channels on CPU against explicit shader evidence.

Run inside Blender with --extractor-src, --manifest, --shader-evidence, --output,
and optional --adapter. Shader evidence is an external JSON receipt containing
assembly, parameterLayout, shaderDefaults, and sourceMaterial identities.
"""
import argparse
import copy
import hashlib
import importlib
import json
from pathlib import Path
import sys

import bpy
import numpy as np

HERE = Path(__file__).resolve().parent
ENGINE = next((p for p in HERE.parents if (p / 'CMakeLists.txt').is_file()
               and (p / 'tools/blender/bake_material_channels.py').is_file()), None)
if ENGINE is None:
    raise RuntimeError('Run this test from an AzureRender source checkout')
PRODUCTION = ENGINE / 'tools/blender'
parser = argparse.ArgumentParser(description=__doc__)
for argument in ('extractor-src', 'manifest', 'shader-evidence', 'output'):
    parser.add_argument('--' + argument, type=Path, required=True)
parser.add_argument('--adapter', default='native_surface_materials')
parser.add_argument('--color-upload-evidence', type=Path)
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
for name in ('extractor_src', 'manifest', 'shader_evidence'):
    setattr(args, name, getattr(args, name).resolve(strict=True))
args.output = args.output.resolve()
args.output.mkdir(parents=True, exist_ok=True)
sys.path[:0] = [str(PRODUCTION), str(ENGINE / 'tools'), str(args.extractor_src), str(HERE)]
import native_tri_surface_reference as reference_module
from bake_material_channels import bake_material
from endfield_scene_material_contract import load_catalog, reference_key

reference = reference_module.NativeTriSurfaceReference(args.shader_evidence)
evaluate = reference.evaluate
adapter = importlib.import_module(args.adapter)
expected_adapter = ((args.extractor_src if args.adapter == 'blender_scene_materials' else PRODUCTION)
                    / (args.adapter + '.py')).resolve()
assert Path(adapter.__file__).resolve() == expected_adapter, (adapter.__file__, expected_adapter)
if args.adapter == 'native_spline_materials':
    shared = importlib.import_module('native_surface_materials')
    assert Path(shared.__file__).resolve() == (PRODUCTION / 'native_surface_materials.py').resolve()
upstream = importlib.import_module('blender_scene_materials')
assert Path(upstream.__file__).resolve() == args.extractor_src / 'blender_scene_materials.py'
materials, textures = load_catalog((args.manifest,))
source_material = reference.evidence['sourceMaterial']
identity = (str(source_material['Source']).casefold(), source_material['PathId'])
original = next(r for r in materials.values() if (str(r['Source']).casefold(), r['PathId']) == identity)
bpy.context.scene.render.engine = 'CYCLES'
bpy.context.scene.cycles.device = 'CPU'


def srgb_decode(value):
    return value / 12.92 if value <= .04045 else ((value + .055) / 1.055) ** 2.4


def sample(record, mapping, builder):
    material = builder(record, mapping, True)
    bpy.context.scene.cycles.device = 'CPU'
    baked, receipt = bake_material(material, 8)
    result = {}
    for name in ('base', 'orm'):
        image = bpy.data.images[receipt['images'][name]]
        values = np.asarray(image.pixels[:], dtype=np.float64).reshape((-1, 4))[36].tolist()
        result[name] = values
    result['base'] = [srgb_decode(v) for v in result['base'][:3]]
    result['metallic'], result['roughness'] = result['orm'][2], result['orm'][1]
    result['device'] = bpy.context.scene.cycles.device
    return result


def fixture(record):
    values = {'_BaseColorMap':[51,102,179,89], '_NormalMap':[128,128,102,255], '_MaskMap':[179,128,77,255]}
    mapping = dict(textures)
    for prop, rgba in values.items():
        image = bpy.data.images.new(prop + '-tri-fixture', width=8, height=8, alpha=True)
        image.colorspace_settings.name = 'Non-Color'
        image.alpha_mode = 'CHANNEL_PACKED'
        image.pixels[:] = [v/255 for v in rgba] * 64
        image.filepath_raw = str(args.output / (prop + '.png'))
        image.file_format = 'PNG'
        image.save()
        slot = next(s for s in record['Textures'] if s['Property'] == prop)
        slot['Scale'], slot['Offset'] = [1,1], [0,0]
        mapping[reference_key(slot['Texture'])] = (args.output, prop + '.png')
    record['Floats'].update(_BaseUVSet=0, _BasePbrMapUVSet=0, _MaskUVSet=0,
        _RoughnessMin=.2, _RoughnessMax=.8, _Metallic=.23,
        _MaskMetallicR=.8, _MaskMetallicG=.4, _MaskMetallicB=.6,
        _MaskRoghnessR=.3, _MaskRoghnessG=.7, _MaskRoghnessB=.1,
        _MaskRScale=1, _MaskROffset=-.2, _MaskGScale=.5, _MaskGOffset=.1,
        _MaskBScale=2, _MaskBOffset=.2, _BaseColorTintCover=.2, _BaseColorBrighterScale=1.3)
    record['Colors'].update(_BaseColor=[.8,.6,.4,1], _MaskAlbedoR=[.9,.1,.2,.5],
        _MaskAlbedoG=[.1,.8,.2,.4], _MaskAlbedoB=[.2,.3,.9,.75])
    base = [srgb_decode(v/255) for v in values['_BaseColorMap'][:3]] + [values['_BaseColorMap'][3]/255]
    return mapping, base, [v/255 for v in values['_NormalMap']], [v/255 for v in values['_MaskMap'][:3]]


results, failures = [], []
if args.color_upload_evidence:
    color_upload=json.loads(args.color_upload_evidence.read_text(encoding='utf-8'))
    assert color_upload['schemaVersion']==1
    for case in color_upload['cases']:
        record=copy.deepcopy(original)
        mapping,base,nro,mask=fixture(record)
        record['Colors'].update(case['authoredColors'])
        gpu_record=copy.deepcopy(record);gpu_record['Colors'].update(case['gpuColors'])
        expected=evaluate(gpu_record,base,nro,mask)
        def authored_builder(record,mapping,pack):
            return adapter.create_material(record,mapping,pack,color_encoding='authored-srgb')
        actual=sample(record,mapping,authored_builder)
        for channel in ('base','metallic','roughness'):
            if np.max(np.abs(np.asarray(actual[channel])-np.asarray(expected[channel])))>.012:
                failures.append(f'captured upload {case["event"]} {channel} differs')
        results.append(dict(case='captured-color-upload-'+str(case['event']),expected=expected,actual=actual))

for mode, saturation in ((1, False), (2, False), (1, True)):
    record = copy.deepcopy(original)
    mapping, base, nro, mask = fixture(record)
    record['Floats']['_BaseTextureMapCount'] = mode
    if saturation:
        record['Floats'].update(_MaskROffset=.2, _MaskBOffset=-.2)
    expected = evaluate(record, base, nro, mask)
    actual = sample(record, mapping, adapter.create_material)
    for channel in ('base', 'metallic', 'roughness'):
        a, e = np.asarray(actual[channel]), np.asarray(expected[channel])
        if np.max(np.abs(a-e)) > .012:
            failures.append(f'mode {mode} saturation={saturation} {channel}: {a.tolist()} != {e.tolist()}')
    results.append({'case':f'overlap-scale-offset-alpha-mode-{mode}-saturation-{saturation}', 'expected':expected, 'actual':actual})

# Exercise source PNG bytes at explicit texel centers, independently of mesh UVs.
record = copy.deepcopy(original)
mapping = dict(textures)
sample_values = {}
for prop in ('_BaseColorMap', '_NormalMap', '_MaskMap'):
    slot = next(s for s in record['Textures'] if s['Property'] == prop)
    root, relative = mapping[reference_key(slot['Texture'])]
    image = bpy.data.images.load(str(root / relative.replace('\\', '/')), check_existing=False)
    image.colorspace_settings.name = 'Non-Color'
    image.alpha_mode = 'CHANNEL_PACKED'
    pixels = np.asarray(image.pixels[:],dtype=np.float64).reshape((image.size[1],image.size[0],4))
    # The actual mask maximum ensures this fixture exposes the omitted metallic.
    if prop == '_MaskMap':
        index = np.unravel_index(np.argmax(pixels[:,:,0]), pixels[:,:,0].shape)
    elif prop == '_NormalMap':
        index = np.unravel_index(np.argmax(pixels[:,:,2]), pixels[:,:,2].shape)
    else:
        index = (100,100)
    y,x = int(index[0]),int(index[1])
    sample_values[prop] = pixels[y,x].tolist()
    slot['Scale'],slot['Offset'] = [0,0], [(x+.5)/image.size[0],(y+.5)/image.size[1]]
    slot['Texture'].setdefault('TextureSampler',{}).update(FilterMode=0,WrapU=0,WrapV=0)
base = [srgb_decode(v) for v in sample_values['_BaseColorMap'][:3]] + [sample_values['_BaseColorMap'][3]]
expected = evaluate(record, base, sample_values['_NormalMap'],sample_values['_MaskMap'][:3])
actual = sample(record, mapping, adapter.create_material)
for channel in ('base','metallic','roughness'):
    if np.max(np.abs(np.asarray(actual[channel])-np.asarray(expected[channel]))) > .012:
        failures.append(f'real source {channel}: {actual[channel]} != {expected[channel]}')
results.append({'case':original['Name'] + '-real-source-texels','rawSourceValues':sample_values,'expected':expected,'actual':actual})

for case in ('disabled', 'three-texture'):
    record = copy.deepcopy(original)
    mapping, _, _, _ = fixture(record)
    if case == 'disabled':
        record['Floats']['_EnableTriChannelMask'] = 0
        record['RenderState']['ValidKeywords'] = ['_TWO_BASEMAP']
    else:
        record['Floats']['_BaseTextureMapCount'] = 0
        record['RenderState']['ValidKeywords'] = ['_TRI_CHANNEL_MASK']
    expected = sample(record, mapping, upstream.create_material)
    actual = sample(record, mapping, adapter.create_material)
    for channel in ('base','metallic','roughness'):
        if np.max(np.abs(np.asarray(actual[channel])-np.asarray(expected[channel]))) > 1e-6:
            failures.append(f'{case} compatibility: {channel}')
    results.append({'case':case+'-compatibility','expected':expected,'actual':actual})

report = {'schema':'NativeTriSurfaceCpuIntegration/1','adapter':args.adapter,
          'adapterPath':str(expected_adapter), 'adapterSha256':hashlib.sha256(expected_adapter.read_bytes()).hexdigest(),
          'production':True,'harnessSha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
          'device':'CPU','gpuExecuted':False,'results':results,'failures':failures,
          'manifest':{'path':str(args.manifest),'sha256':hashlib.sha256(args.manifest.read_bytes()).hexdigest()},
          'upstream':{'path':str(Path(upstream.__file__).resolve()),
                      'sha256':hashlib.sha256(Path(upstream.__file__).read_bytes()).hexdigest()},
          'shaderEvidence':{'path':str(args.shader_evidence),'sha256':hashlib.sha256(args.shader_evidence.read_bytes()).hexdigest(),
                            'inputs':reference.inputs},
          'referenceModule':{'path':str(Path(reference_module.__file__).resolve()),
                             'sha256':hashlib.sha256(Path(reference_module.__file__).read_bytes()).hexdigest()},
          'limits':['Real-number instruction arithmetic; GPU rounding and sampling LOD are outside this test.',
                    'Source texture texels are fixed independently; mesh UV correlation and runtime binding remain pending.']}
(args.output / 'result.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2))
assert not failures, '\n'.join(failures)
