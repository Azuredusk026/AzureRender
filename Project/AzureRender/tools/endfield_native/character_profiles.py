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
from pathlib import Path
import sys

import numpy as np
from PIL import Image


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
        else:
            features.add('stylized-shadow')
            if 'metallicRoughnessTexture' in material.get('pbrMetallicRoughness', {}):
                features.add('surface-ao')
        if floats.get('_EnableOutline') == 1 and 'CharacterOutline' not in native.get('disabledShaderPasses', []):
            features.add('silhouette-outline')
        row = {'name': material['name'], 'source': source['object'], 'class': kind, 'bound': [],
            'sourceKeywords': sorted(keywords), 'sourceFloats': floats, 'sourceColors': colors,
            'remainingNativeShaderInputs': []}

        def source_payload(slot):
            if slot not in bindings:
                raise ValueError('Missing active source slot: ' + slot)
            binding = bindings[slot]
            ref = binding['reference']['resolved']
            if identity(ref) not in payloads:
                raise ValueError('Missing source image: ' + slot)
            payload = payloads[identity(ref)]
            if binding['scale'] != [1, 1] or binding['offset'] != [0, 0]:
                raise ValueError('Character data requires an explicit supported UV transform: ' + slot)
            return payload, binding, ref

        def bind_texture(slot, target):
            payload, binding, ref = source_payload(slot)
            if binding['color_space'] != 0:
                raise ValueError('Character data texture must be linear: ' + slot)
            index = append_texture(payload, ref['name'])
            row['bound'].append({'slot': slot, 'target': target, 'source': ref,
                'sourcePngSha256': hashlib.sha256(payload).hexdigest(), 'texture': index})
            return index

        if kind == 'hair':
            features.add('hair-anisotropy')
            hair = profile.setdefault('hair', {'power': 200, 'strength': floats.get('_AnisotropyIntensity', 1),
                'rampRow': 3, 'strandShift': .1, 'cameraOffset': [0, 0, 0], 'upperLimit': 4})
            if '_SPECULAR_NORMALMAP' in keywords and floats.get('_UseSpecBumpMap') == 1:
                hair['dataTexture'] = bind_texture('_SplitNormalMap', 'hair.dataTexture')
                hair['dataNormalY'] = 'positive'
            row['hostHairParameters'] = 'Engine dual-lobe power/shift defaults; native anisotropy intensity supplies strength'
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
