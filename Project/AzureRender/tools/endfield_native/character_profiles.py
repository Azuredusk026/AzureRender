"""Complete explicit character material profiles from the pinned native audit.

The output preserves original animation and geometry. Bound inputs and source
properties are reported separately from native shader equivalence.
"""
import argparse
import base64
import copy
import hashlib
from io import BytesIO
import json
import math
from pathlib import Path
import sys

import numpy as np
from PIL import Image


# Pinned source shader; pass 1 programs 10/11 use Zero/SrcColor RGB blending.
# Matching a material name or a grayscale mask does not establish this contract.
MULTIPLY_OVERLAY_SHADER = ('CAB-da44c449ed4aff035ab44ec6c74bdbfe', 1962491951764993823)
PROJECTED_HAIR_SHADER = ('CAB-623a5cbbe298e3f04194d1e14a05ebfe', -8095970123935614097)


def material_class(name):
    name = name.lower()
    if 'hairshadow' in name or 'eyeshadow' in name or 'eyewhiteshadow' in name:
        return 'overlay'
    for marker, kind in (('hair', 'hair'), ('face', 'face'), ('iris', 'eye'),
                         ('brow', 'skin'), ('body', 'skin'), ('cloth', 'fabric')):
        if marker in name:
            return kind
    return 'generic'


def restore_profiles(document, audit, sources, payloads):
    result = copy.deepcopy(document)
    source_by_name = {row['object']['name']: row for row in audit['materials']}
    if len(source_by_name) != len(audit['materials']):
        raise ValueError('Ambiguous source material names')
    identity = lambda row: (row['source'], row['path_id'])
    raw = {identity(row['object']): row['data'] for row in sources if row.get('available', True)}
    report = {'materials': [], 'nativeVisualEquivalenceVerified': False,
        'scope': 'Explicit material semantics, source strand data, linear diffuse ramps and gray-mask overlays'}
    images = result.setdefault('images', [])
    textures = result.setdefault('textures', [])
    cache = {}

    def append_texture(payload, name):
        digest = hashlib.sha256(payload).hexdigest()
        if digest not in cache:
            images.append({'name': name, 'uri': 'data:image/png;base64,' + base64.b64encode(payload).decode()})
            textures.append({'source': len(images) - 1})
            cache[digest] = len(textures) - 1
        return cache[digest]

    for material in result.get('materials', []):
        source = source_by_name.get(material.get('name', ''))
        if source is None:
            continue
        native = raw[identity(source['object'])]
        extras = material.setdefault('extras', {})
        if 'nativeOpaqueSource' in extras and identity(extras['nativeOpaqueSource']) != identity(source['object']):
            raise ValueError('Target material identity disagrees with source audit')
        kind = material_class(source['object']['name'])
        if kind == 'generic':
            continue
        floats = {row['name']: row['value'] for row in source['floats']}
        colors = {row['name']: row['rgba'] for row in source['colors']}
        bindings = {row['slot']: row for row in source['textures'] if row['reference']['resolved']}
        keywords = set(native.get('m_ValidKeywords', []))
        profile = extras.setdefault('azureRenderMaterial', {'schemaVersion': 2, 'class': kind})
        profile.update(schemaVersion=2, **{'class': kind})
        features = set(profile.get('features', []))
        features.discard('neutral-fallback')
        if kind == 'overlay':
            features = {'overlay'}
            profile.pop('composite', None)
        else:
            features.add('stylized-shadow')
            if 'metallicRoughnessTexture' in material.get('pbrMetallicRoughness', {}):
                features.add('surface-ao')
        if floats.get('_EnableOutline') == 1 and 'CharacterOutline' not in native.get('disabledShaderPasses', []):
            features.add('silhouette-outline')
        row = {'name': material['name'], 'source': source['object'], 'class': kind, 'bound': [],
            'sourceKeywords': sorted(keywords), 'sourceFloats': floats, 'sourceColors': colors,
            'remainingNativeShaderInputs': []}

        def source_payload(slot, allow_transform=False):
            if slot not in bindings:
                raise ValueError('Missing active source slot: ' + slot)
            binding = bindings[slot]
            ref = binding['reference']['resolved']
            if identity(ref) not in payloads:
                raise ValueError('Missing source image: ' + slot)
            payload = payloads[identity(ref)]
            if not allow_transform and (binding['scale'] != [1, 1] or binding['offset'] != [0, 0]):
                raise ValueError('Character data requires an explicit supported UV transform: ' + slot)
            return payload, binding, ref

        def bind_texture(slot, target, allow_transform=False):
            payload, binding, ref = source_payload(slot, allow_transform)
            if binding['color_space'] != 0:
                raise ValueError('Character data texture must be linear: ' + slot)
            index = append_texture(payload, ref['name'])
            row['bound'].append({'slot': slot, 'target': target, 'source': ref,
                'sourcePngSha256': hashlib.sha256(payload).hexdigest(), 'texture': index})
            return index

        if kind == 'hair':
            features.add('hair-anisotropy')
            hair = profile.setdefault('hair', {})
            for key, value in {'power': 200, 'strength': floats.get('_AnisotropyIntensity', 1),
                'rampRow': 3, 'strandShift': .1, 'cameraOffset': [0, 0, 0], 'upperLimit': 4}.items():
                hair.setdefault(key, value)
            features.discard('hair-projected-ramp')
            hair.pop('projectedRamp', None)
            if '_SPECULAR_NORMALMAP' in keywords and floats.get('_UseSpecBumpMap') == 1:
                hair['dataTexture'] = bind_texture('_SplitNormalMap', 'hair.dataTexture')
                hair['dataNormalY'] = 'positive'
            row['hostHairParameters'] = 'Engine dual-lobe power/shift defaults; native anisotropy intensity supplies strength'
            shader = source.get('shader', {}).get('resolved')
            if (shader and identity(shader) == PROJECTED_HAIR_SHADER
                    and '_SPEC_RAMP_ON' in keywords and floats.get('_UseSpecRampMap') == 1):
                if floats.get('_BackFaceNormalFlip') != 0:
                    raise ValueError('Unsupported projected hair back-face normal convention')
                source_fields = {'_AnisotropyValue': 'primaryShift', '_AnisotropyValue2': 'secondaryShift',
                    '_AnisotropyIntensity': 'intensity', '_AnisotropyRange2': 'secondaryRange',
                    '_AnisotropyEdgeFade': 'edgeFade', '_SpecBumpScale': 'specularNormalScale',
                    '_BumpScale': 'surfaceNormalScale', '_AnisotropyDirX': 'directionX'}
                bounds = {'primaryShift': (-1, 1), 'secondaryShift': (-1, 1),
                    'intensity': (0, 16), 'secondaryRange': (0, 1), 'edgeFade': (0, 64),
                    'specularNormalScale': (0, 8), 'surfaceNormalScale': (0, 8), 'directionX': (-16, 16)}
                projected = {'schemaVersion': 1}
                for original, target in source_fields.items():
                    if original not in floats:
                        raise ValueError('Missing active projected hair field: ' + original)
                    value = floats[original]
                    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
                        raise ValueError('Invalid active projected hair field: ' + original)
                    projected[target] = 2 * value - 1 if target in ('primaryShift', 'secondaryShift') else value
                    if not bounds[target][0] <= projected[target] <= bounds[target][1]:
                        raise ValueError('Projected hair field is outside supported ranges: ' + original)
                color = colors.get('_AnisotropyColor2')
                if color is None:
                    raise ValueError('Missing active projected hair field: _AnisotropyColor2')
                if (len(color) != 4 or any(isinstance(value, bool) or not isinstance(value, (int, float))
                        or not math.isfinite(value) for value in color)):
                    raise ValueError('Invalid active projected hair field: _AnisotropyColor2')
                if any(value < 0 or value > 64 for value in color[:3]):
                    raise ValueError('Projected hair field is outside supported ranges: _AnisotropyColor2')
                # This pinned ShaderLab Color property is stored in sRGB.
                # Native draw 950 cb6[15] confirms linear upload; texture
                # color-space flags are independent from uniform colors.
                linear_color=[value/12.92 if value<=.04045 else ((value+.055)/1.055)**2.4
                    for value in color[:3]]
                if any(value>64 for value in linear_color):
                    raise ValueError('Projected hair linear color exceeds supported range')
                if '_SPECULAR_NORMALMAP' not in keywords or floats.get('_UseSpecBumpMap') != 1:
                    raise ValueError('Missing active projected hair field: independent split normal')
                ramp_payload, _, _ = source_payload('_SpecRampMap')
                with Image.open(BytesIO(ramp_payload)) as image:
                    if image.width < 2 or image.height < 2:
                        raise ValueError('Projected hair ramp requires two dimensions')
                projected.update(parameterTexture=bind_texture('_MetallicGlossMap', 'hair.projectedRamp.parameterTexture'),
                    rampTexture=bind_texture('_SpecRampMap', 'hair.projectedRamp.rampTexture'),
                    secondaryColor=linear_color, objectBasis=[1, 0, 0, 0, 1, 0, 0, 0, 1])
                features.discard('hair-ramp')
                features.discard('face-sdf-eligible')
                profile.pop('faceSdf', None)
                hair.pop('rampTexture', None)
                features.add('hair-projected-ramp')
                hair['projectedRamp'] = projected
                # This pinned program uses P.R for its strand axis and P.A for
                # its secondary lobe. Neither is a metallic/roughness input.
                pbr = material.setdefault('pbrMetallicRoughness', {})
                pbr.pop('metallicRoughnessTexture', None)
                pbr.update(metallicFactor=0, roughnessFactor=1)
                features.discard('surface-ao')
                projected.update(surfaceResponse='strand-diffuse', diffuseScale=.96)
                if '_SPECULAR_LINE' in keywords and floats.get('_SpecularLine')==1:
                    fields={'_LineAmount':('frequency',0,65536),'_LineValue':('strandShift',0,1),
                        '_LineRange':('range',0,1),'_LineIntensity':('depth',0,1),
                        '_UseLineMap':('alternation',0,1),'_LineSaturation':('saturation',0,16)}
                    line={'schemaVersion':1}
                    for key,(target,minimum,maximum) in fields.items():
                        value=floats.get(key)
                        if isinstance(value,bool) or not isinstance(value,(int,float)) or not math.isfinite(value) or not minimum<=value<=maximum:
                            raise ValueError('Invalid active specular line field: '+key)
                        line[target]=2*value-1 if target=='strandShift' else value
                    _,binding,_=source_payload('_LineMap',True)
                    transform=list(binding['scale'])+list(binding['offset'])
                    if len(transform)!=4 or any(isinstance(v,bool) or not isinstance(v,(int,float)) or not math.isfinite(v) or abs(v)>65536 for v in transform):
                        raise ValueError('Invalid specular line UV transform')
                    line.update(texture=bind_texture('_LineMap','hair.projectedRamp.line.texture',True),uvTransform=transform)
                    projected['line']=line
                row['projectedHairSource'] = {'shader': shader, 'pass': 'ForwardLit', 'program': 263,
                    'parameterBlob': 45, 'objectBasisStatus': 'identity-candidate-runtime-frame-unverified',
                    'normalConvention': {'sourceBackFaceNormalFlip': 0,
                        'surfaceBackFaceSign': -1, 'highlightBackFaceSign': 1,
                        'surfaceLines': [164, 172], 'highlightLines': [177, 190]},
                    'shiftConversion': '2 * source anisotropy value - 1',
                    'secondaryColorConversion': {'source':'sRGB-uniform','target':'linear',
                        'sourceRgb':list(color[:3]),'linearRgb':linear_color},
                    'textureSemantics': {'parameterTexture': 'linear-original-RGBA',
                        'rampTexture': 'linear-original-2D-RGB-LOD0'},
                    'surfaceSemantics': {'metallic': 0, 'diffuseScale': .96,
                        'roughnessStatus': 'host-compatibility-placeholder',
                        'genericDirectSpecular': False, 'genericEnvironmentSpecular': False,
                        'parameterB': 'source-shadow-ramp-screen-rim-consumers-pending',
                        'evidence': {'diffuseScaleLines': [474, 476, 901, 906, 1078, 1134],
                            'parameterBLines': [868, 920, 1075],
                            'parameterALines': [1018, 1054]}}}
                row['remainingProjectedHairInputs'] = ['main-virtual-light-global-bindings', 'SPECULAR_LINE',
                    'object-basis-runtime-verification', 'source-sampler-runtime-binding',
                    'object-highlight-gain', 'direct-light-global-constants',
                    'parameter-B-shadow-ramp-screen-rim']
                if 'line' in projected:row['remainingProjectedHairInputs'].remove('SPECULAR_LINE')
                row['hostHairParameters'] = 'Projected ramp consumes source fields; legacy fields configure the fallback contract'
        if '_DIFF_RAMP_ON' in keywords and floats.get('_UseDiffRampMap') == 1 and kind != 'face':
            index = bind_texture('_DiffRampMap', 'diffuseRamp.texture')
            payload = payloads[identity(bindings['_DiffRampMap']['reference']['resolved'])]
            with Image.open(BytesIO(payload)) as image:
                if image.height != 1 or image.width < 2:
                    raise ValueError('Diffuse ramp must contain one horizontal row')
            profile['diffuseRamp'] = {'schemaVersion': 1, 'texture': index}
            features.add('diffuse-ramp')
        if kind == 'overlay' and floats.get('_UseGrayAsAlpha') == 1:
            payload, _, ref = source_payload('_BaseMap')
            pixels = np.asarray(Image.open(BytesIO(payload)).convert('RGBA')).copy()
            tint = np.clip(np.asarray(colors.get('_BaseColor', [1, 1, 1, 1]), dtype=float), 0, 1)
            rgb = np.where(tint[:3] <= .0031308, tint[:3] * 12.92, 1.055 * tint[:3] ** (1 / 2.4) - .055)
            pixels[..., 3] = np.rint(pixels[..., 0] * tint[3]).astype(np.uint8)
            pixels[..., :3] = np.rint(rgb * 255).astype(np.uint8)
            stream = BytesIO()
            Image.fromarray(pixels, 'RGBA').save(stream, format='PNG')
            index = append_texture(stream.getvalue(), ref['name'] + '-tinted-alpha')
            material.setdefault('pbrMetallicRoughness', {}).update(
                baseColorTexture={'index': index}, baseColorFactor=[1, 1, 1, 1])
            material['alphaMode'] = 'BLEND'
            row['bound'].append({'slot': '_BaseMap', 'target': 'baseColorTexture:tint-rgb/source-red-alpha',
                'source': ref, 'sourcePngSha256': hashlib.sha256(payload).hexdigest(),
                'outputPngSha256': hashlib.sha256(stream.getvalue()).hexdigest(), 'texture': index})
            shader = source.get('shader', {}).get('resolved')
            if shader and identity(shader) == MULTIPLY_OVERLAY_SHADER:
                features.add('multiply-composite')
                profile['composite'] = {'schemaVersion': 1, 'mode': 'multiply'}
                material['doubleSided'] = False
                row['cullingSource'] = {'shader': shader, 'pass': 1, 'programs': [10, 11],
                    'culling': {'value': 2, 'mode': 'Back'},
                    'source': 'OverlayShadow shader pass state'}
                row['compositeSource'] = {'shader': shader, 'pass': 1, 'programs': [10, 11],
                    'colorBlend': {'source': 'Zero', 'destination': 'SrcColor'},
                    'hostAlpha': 'preserve-destination'}
                row['remainingNativeCompositeInputs'] = ['stencil', 'screen-gbuffer-suppression',
                    'pre-depth', 'light-suppression', 'fog', 'vfx-grading', 'shadow-angle-offset']
            else:
                row['remainingNativeCompositeInputs'] = ['unverified-overlay-shader']
        profile['features'] = sorted(features)
        consumed = {entry['slot'] for entry in row['bound']}
        row['remainingNativeShaderInputs'] = [slot for slot in bindings if slot not in consumed
            and slot not in ('_BaseMap', '_BumpMap', '_MetallicGlossMap', '_EmissionMap',
                             '_SDFLightmap', '_SDFMask', '_ShadowLutTex')
            and not (kind == 'face' and slot == '_DiffRampMap' and 'faceSdf' in profile)]
        extras['nativeCharacterProfileSource'] = {'material': source['object'],
            'floats': floats, 'colors': colors, 'keywords': sorted(keywords)}
        report['materials'].append(row)
    return result, report


def main():
    parser = argparse.ArgumentParser(__doc__)
    for name in ('input', 'source-root', 'output', 'report'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists() or args.report.exists() or args.output.resolve() == args.report.resolve():
        raise ValueError('Outputs must be distinct new files')
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from convert_material_semantics import read_asset, write_asset
    names = ('source_audit.json', 'material_sources.json', 'source_texture_artifacts.json')
    packets = {name: json.loads((args.source_root / name).read_text(encoding='utf8')) for name in names}
    payloads = {}
    for row in packets[names[2]]:
        path = (args.source_root / row['png']['path']).resolve()
        if not path.is_relative_to(args.source_root.resolve()):
            raise ValueError('Source image escapes source root')
        payload = path.read_bytes()
        if hashlib.sha256(payload).hexdigest().lower() != row['png']['sha256'].lower():
            raise ValueError('Source image digest mismatch')
        payloads[(row['object']['source'], row['object']['path_id'])] = payload
    document, tail = read_asset(args.input)
    output, report = restore_profiles(document, packets[names[0]], packets[names[1]], payloads)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    write_asset(args.output, output, tail)
    verified, verified_tail = read_asset(args.output)
    if verified != output or verified_tail != tail:
        raise AssertionError('Character profile output changed geometry or binary data')
    report.update(input=str(args.input.resolve()), inputSha256=hashlib.sha256(args.input.read_bytes()).hexdigest(),
        output=str(args.output.resolve()), outputSha256=hashlib.sha256(args.output.read_bytes()).hexdigest(),
        sourceFiles={name: hashlib.sha256((args.source_root / name).read_bytes()).hexdigest() for name in names},
        generatorSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), binaryTailPreserved=True)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2), encoding='utf8')
    print(json.dumps({'materials': len(report['materials']), 'binaryTailPreserved': True}))


if __name__ == '__main__':
    main()
