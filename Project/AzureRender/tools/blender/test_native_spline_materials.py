"""Blender integration checks for source RGBA emission and effect opacity.

Requires an explicit external extractor and its scene manifest.
"""
import argparse
import copy
import importlib
import json
from pathlib import Path
import sys

import bpy

parser = argparse.ArgumentParser()
parser.add_argument('--extractor-src', type=Path, required=True)
parser.add_argument('--manifest', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--adapter', default='native_spline_materials')
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
args.output = args.output.resolve()
args.manifest = args.manifest.resolve()
args.extractor_src = args.extractor_src.resolve()
sys.path[:0] = [str(Path(__file__).parent), str(args.extractor_src)]
from bake_material_channels import bake_material
from endfield_scene_material_contract import load_catalog, reference_key

create_material = importlib.import_module(args.adapter).create_material
args.output.mkdir(parents=True, exist_ok=True)
materials, textures = load_catalog((args.manifest,))
fixture = bpy.data.images.new('FixtureRGBA', width=8, height=8, alpha=True)
fixture.colorspace_settings.name = 'Non-Color'
fixture.alpha_mode = 'STRAIGHT'
fixture.pixels[:] = [.1, .4, .8, .25] * 64
fixture.filepath_raw = str(args.output / 'rgba.png')
fixture.file_format = 'PNG'
fixture.save()


def replace_texture(record, prop):
    slot = next(s for s in record['Textures'] if s['Property'] == prop)
    slot['Texture']['ColorSpace'] = 0
    slot['Scale'], slot['Offset'] = [1, 1], [0, 0]
    textures[reference_key(slot['Texture'])] = (args.output, 'rgba.png')


def sample(record):
    material = create_material(record, textures, True)
    _, receipt = bake_material(material, 8)
    values = {}
    for key in ('base', 'emission'):
        image = bpy.data.images[receipt['images'][key]]
        values[key] = list(image.pixels)[(4 * 8 + 4) * 4:(4 * 8 + 4) * 4 + 4]
        values[key][:3] = [v / 12.92 if v <= .04045 else ((v + .055) / 1.055) ** 2.4
                           for v in values[key][:3]]
    values['emission'][:3] = [v * receipt['emissionScale'] for v in values['emission'][:3]]
    return values


results = []
failures = []
for family, expected in [('HGRP/LitEffect', [.2, .45, .95]),
                         ('HGRP/Effect/VFXBaseV2', [.1, .4, .8])]:
    record = copy.deepcopy(next(r for r in materials.values() if r['Shader']['Name'] == family))
    if family == 'HGRP/LitEffect':
        replace_texture(record, '_EmissiveMap')
        record['Colors']['_EmissiveColorA'] = [.4, .2, .6, 1]
    else:
        replace_texture(record, '_MainTex')
        record['Floats'].update(_DisableVertColor=1, _TintColorIntensity=1)
        record['Colors']['_TintColor'] = [1, 1, 1, 1]
    actual = sample(record)
    results.append({'family': family, 'actual': actual, 'expectedEmission': expected})
    if max(abs(a - b) for a, b in zip(actual['emission'], expected)) > .015:
        failures.append(f'{family} RGBA emission: {actual["emission"]} != {expected}')
    if family.endswith('VFXBaseV2') and abs(actual['base'][3] - .25) > .015:
        failures.append(f'effect opacity: {actual["base"][3]} != .25')
corners = bpy.data.images.new('WrapCorners', width=2, height=2, alpha=True)
corners.colorspace_settings.name = 'Non-Color'
corners.pixels[:] = [1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1]
corners.filepath_raw = str(args.output / 'wrap.png')
corners.file_format = 'PNG'
corners.save()
for wrap, uv, expected in [((1, 0), (-.25, 1.75), [0, 0, 1]),
                            ((1, 0), (1.25, -.75), [0, 1, 0]),
                            ((0, 1), (1.75, -.25), [0, 1, 0]),
                            ((1, 0), (-2, 1), [.5, 0, .5])]:
    record = copy.deepcopy(next(r for r in materials.values()
                               if r['Name'] == 'M_fx_base01_lv001_ship_line_1124'))
    record['Floats'].update(_DisableVertColor=1, _TintColorIntensity=1)
    record['Colors']['_TintColor'] = [1, 1, 1, 1]
    slot = next(s for s in record['Textures'] if s['Property'] == '_MainTex')
    slot['Scale'], slot['Offset'] = [0, 0], list(uv)
    reference = slot['Texture']
    reference['ColorSpace'] = 0
    reference['TextureSampler'].update(WrapU=wrap[0], WrapV=wrap[1], FilterMode=1)
    textures[reference_key(reference)] = (args.output, 'wrap.png')
    actual = sample(record)['emission']
    results.append({'wrap': wrap, 'uv': uv, 'actual': actual, 'expected': expected})
    if max(abs(a - b) for a, b in zip(actual, expected)) > .015:
        failures.append(f'per-axis sampler {wrap} at {uv}: {actual} != {expected}')
record = copy.deepcopy(next(r for r in materials.values()
                           if r['Name'] == 'M_fx_base01_lv001_ship_line_1124'))
replace_texture(record, '_MainTex')
record['Floats'].update(_DisableVertColor=1, _TintColorIntensity=1, _ExpIntensity=2, _ExpThreshold=.2)
record['Colors']['_TintColor'] = [1, 1, 1, 1]
actual = sample(record)['emission']
results.append({'case': 'HDR exposure gain', 'actual': actual, 'expected': [.1, .8, 2]})
if max(abs(a - b) for a, b in zip(actual, [.1, .8, 2])) > .035:
    failures.append(f'HDR effect gain: {actual} != [.1,.8,2]')
for field, value in [('_BlendMode', 1), ('_ZWrite', 1)]:
    invalid = copy.deepcopy(record)
    invalid['Floats'][field] = value
    try:
        create_material(invalid, textures, True)
    except ValueError:
        pass
    else:
        failures.append(f'unsupported source state silently accepted: {field}')
(args.output / 'result.json').write_text(json.dumps({'results': results, 'failures': failures}, indent=2))
assert not failures, '\n'.join(failures)
print('PASS: source RGBA emission and effect opacity')
