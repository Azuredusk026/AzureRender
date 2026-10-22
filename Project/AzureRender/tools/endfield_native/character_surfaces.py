"""Exchange decoded native character surface channels through standard glTF.

Channel reference: pinned reader character_runtime/rebuild_character_materials.py,
decode_normal and material_group. This exchanges surface data, not NPR lighting.
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


def identity(row):
    return row['source'], row['path_id']


def normal_image(image, strength, layout):
    if layout not in ('unity-rag', 'hair-rg') or not math.isfinite(strength) or strength < 0:
        raise ValueError('Unsupported normal layout or strength')
    value = np.asarray(image.convert('RGBA'), dtype=np.float64) / 255
    x = 2 * value[..., 0] * (value[..., 3] if layout == 'unity-rag' else 1) - 1
    y = 2 * value[..., 1] - 1
    if layout == 'hair-rg': x, y = x * strength, y * strength
    z = np.sqrt(np.maximum(1 - x*x - y*y, 0))
    if layout == 'unity-rag': x, y = x * strength, y * strength
    vector = np.stack((x, y, z), axis=-1)
    vector /= np.maximum(np.linalg.norm(vector, axis=-1, keepdims=True), 1e-30)
    pixels = np.rint(np.clip(vector * .5 + .5, 0, 1) * 255).astype(np.uint8)
    return Image.fromarray(pixels, 'RGB').convert('RGBA')


def orm_image(image):
    red, _, blue, alpha = image.convert('RGBA').split()
    roughness = alpha.point(lambda value: 255-value)
    return Image.merge('RGBA', (blue, roughness, red, Image.new('L', image.size, 255)))


def orm_specular_image(image):
    """Pack native AO/roughness/metallic while preserving specular in alpha.

    This opt-in layout stores the original green byte without scaling it.
    The consuming material must explicitly declare the alpha input semantics.
    """
    red, green, blue, alpha = image.convert('RGBA').split()
    roughness = alpha.point(lambda value: 255-value)
    return Image.merge('RGBA', (blue, roughness, red, green))


def color_image(image, color_space):
    if color_space == 1: return image.convert('RGBA')
    if color_space != 0: raise ValueError('Unknown native image color space')
    value = np.asarray(image.convert('RGBA'), dtype=np.float64) / 255
    rgb = value[..., :3]
    value[..., :3] = np.where(rgb <= .0031308, 12.92*rgb, 1.055*np.power(rgb, 1/2.4)-.055)
    return Image.fromarray(np.rint(value*255).astype(np.uint8), 'RGBA')


def restore_surfaces(document, audit, sources, payloads, *, specular_materials=(), specular_only=False):
    result = copy.deepcopy(document)
    selected=set(specular_materials)
    if specular_only and not selected:raise ValueError('Specular-only conversion requires an explicit selection')
    names=[m.get('name','') for m in result.get('materials',[])]
    if any(names.count(name)!=1 for name in selected):
        raise ValueError('Specular selection requires unique existing material names')
    by_name = {row['object']['name']: row for row in audit['materials']}
    if len(by_name) != len(audit['materials']): raise ValueError('Ambiguous native material names')
    raw = {identity(row['object']): row['data'] for row in sources if row.get('available', True)}
    report = {'materials': [], 'shader_equivalence_verified': False,
              'scope': 'Decoded normal, metallic/roughness and emission surface exchange; native lighting remains unbound'}
    images = result.setdefault('images', []); textures = result.setdefault('textures', [])
    cache = {}
    def texture(binding, convert, role):
        ref = binding['reference']['resolved']; key = identity(ref)
        if key not in payloads: raise ValueError('Missing source image: ' + repr(key))
        image = Image.open(BytesIO(payloads[key])); output = convert(image)
        stream = BytesIO(); output.save(stream, format='PNG'); payload = stream.getvalue()
        digest = hashlib.sha256(payload).hexdigest()
        if digest not in cache:
            images.append({'name': ref['name']+'-'+role,
                           'uri': 'data:image/png;base64,'+base64.b64encode(payload).decode('ascii')})
            textures.append({'source': len(images)-1}); cache[digest] = len(textures)-1
        info = {'index': cache[digest]}
        if binding['scale'] != [1, 1] or binding['offset'] != [0, 0]:
            info['extensions'] = {'KHR_texture_transform': {'scale': binding['scale'], 'offset': binding['offset']}}
            used = result.setdefault('extensionsUsed', [])
            if 'KHR_texture_transform' not in used: used.append('KHR_texture_transform')
        return info, {'slot': binding['slot'], 'source': ref, 'source_png_sha256': hashlib.sha256(payloads[key]).hexdigest(),
                      'output_png_sha256': digest, 'target': role}
    for material in result.get('materials', []):
        if specular_only and material.get('name') not in selected:continue
        source = by_name.get(material.get('name', ''))
        if source is None:
            if material.get('name') in selected:raise ValueError('Missing selected material source')
            continue
        floats = {v['name']: v['value'] for v in source['floats']}
        colors = {v['name']: v['rgba'] for v in source['colors']}
        keywords = set(raw[identity(source['object'])].get('m_ValidKeywords', []))
        bindings = {v['slot']: v for v in source['textures'] if v['reference']['resolved']}
        row = {'name': material['name'], 'source': source['object'], 'shader': source['shader']['resolved'],
               'bound': [], 'unbound_slots': [], 'parameters': {}}
        pbr = material.setdefault('pbrMetallicRoughness', {})
        specular=material['name'] in selected
        if specular:
            profile=material.get('extras',{}).get('azureRenderMaterial',{})
            if (profile.get('schemaVersion')!=2 or profile.get('class') not in ('generic','fabric','metal')
                    or floats.get('_UseMetallicGlossMap')!=1):
                raise ValueError('Specular selection requires a supported profile and source parameter map')
            if 'specularInput' in profile:raise ValueError('Specular input already authored')
            profile['specularInput']={'schemaVersion':1,'source':'metallic-roughness-alpha','scale':.5}
        if not specular_only and '_Metallic' in floats and '_Smoothness' in floats:
            metallic, smoothness = floats['_Metallic'], floats['_Smoothness']
            if not 0 <= metallic <= 1 or not 0 <= smoothness <= 1: raise ValueError('Out-of-range source surface factor')
            pbr.update(metallicFactor=metallic, roughnessFactor=1-smoothness)
            row['parameters'].update(metallic=metallic, smoothness=smoothness)
        if not specular_only and '_NORMALMAP' in keywords and floats.get('_UseBumpMap') == 1:
            split = '_SPECULAR_NORMALMAP' in keywords
            slot = '_SplitNormalMap' if split else '_BumpMap'
            if slot not in bindings: raise ValueError('Enabled normal lacks source texture: '+material['name'])
            strength = floats['_BumpScale']; layout = 'hair-rg' if split else 'unity-rag'
            info, evidence = texture(bindings[slot], lambda image: normal_image(image, strength, layout), 'normal')
            material['normalTexture'] = info
            evidence.update(layout=layout, strength=strength)
            row['bound'].append(evidence)
        if floats.get('_UseMetallicGlossMap') == 1:
            if '_MetallicGlossMap' not in bindings: raise ValueError('Enabled metallic map lacks source texture')
            info, evidence = texture(bindings['_MetallicGlossMap'], orm_specular_image if specular else orm_image, 'metallicRoughness')
            pbr.update(metallicRoughnessTexture=info, metallicFactor=1, roughnessFactor=1)
            evidence['unconsumed_channels'] = ['source G specular', 'source B AO retained in packed R; surface-ao feature inactive']
            if specular:
                evidence['unconsumed_channels'].remove('source G specular')
                evidence['specular_input']={'source_channel':'G','target_channel':'A','scale':.5}
            row['bound'].append(evidence)
        if not specular_only and '_EMISSION' in keywords:
            if '_EmissionMap' not in bindings: raise ValueError('Enabled emission lacks source image')
            binding = bindings['_EmissionMap']
            info, evidence = texture(binding, lambda image: color_image(image, binding['color_space']), 'emissive')
            factor = [v*floats['_EmissionBrightness'] for v in colors['_EmissionColor'][:3]]
            if any(not math.isfinite(v) or v < 0 for v in factor): raise ValueError('Invalid emission factor')
            peak = max(1, *factor)
            material.update(emissiveTexture=info, emissiveFactor=[v/peak for v in factor])
            if peak > 1:
                material.setdefault('extensions', {})['KHR_materials_emissive_strength'] = {'emissiveStrength': peak}
                used = result.setdefault('extensionsUsed', [])
                if 'KHR_materials_emissive_strength' not in used: used.append('KHR_materials_emissive_strength')
            evidence['linear_factor'] = factor; row['bound'].append(evidence)
        consumed = {v['slot'] for v in row['bound']} | {'_BaseMap'}
        row['unbound_slots'] = [dict(slot=key, source=value['reference']['resolved']) for key, value in bindings.items() if key not in consumed]
        report['materials'].append(row)
    return result, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('input', 'source-root', 'output', 'report'):
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--specular-material',action='append',default=[])
    parser.add_argument('--specular-only',action='store_true')
    args = parser.parse_args()
    if args.output.exists() or args.report.exists() or args.output.resolve() == args.report.resolve():
        raise ValueError('Outputs must be distinct new files')
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from convert_material_semantics import read_asset, write_asset, describe
    paths = {name: args.source_root/name for name in ('source_audit.json', 'material_sources.json', 'source_texture_artifacts.json')}
    packets = {name: json.loads(path.read_text(encoding='utf8')) for name, path in paths.items()}
    payloads = {}
    for row in packets['source_texture_artifacts.json']:
        path = (args.source_root/row['png']['path']).resolve()
        if not path.is_relative_to(args.source_root.resolve()): raise ValueError('Source image escapes source root')
        payload = path.read_bytes()
        if hashlib.sha256(payload).hexdigest().lower() != row['png']['sha256'].lower(): raise ValueError('Source image digest mismatch')
        payloads[identity(row['object'])] = payload
    document, tail = read_asset(args.input)
    result, report = restore_surfaces(document, packets['source_audit.json'], packets['material_sources.json'], payloads,
        specular_materials=args.specular_material,specular_only=args.specular_only)
    for key in ('nodes', 'meshes', 'skins', 'animations', 'accessors', 'bufferViews', 'buffers'):
        if result.get(key) != document.get(key): raise AssertionError('Structural change: '+key)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    write_asset(args.output, result, tail)
    verified, verified_tail = read_asset(args.output)
    if verified_tail != tail: raise AssertionError('Geometry/animation binary changed')
    semantics = describe(verified)
    for image in semantics['images']:
        if 'uri' in image:
            uri = image.pop('uri')
            image['uri_sha256'] = hashlib.sha256(uri.encode()).hexdigest()
            image['embedded'] = uri.startswith('data:')
    report.update(input_sha256=hashlib.sha256(args.input.read_bytes()).hexdigest(),
                  output_sha256=hashlib.sha256(args.output.read_bytes()).hexdigest(),
                  generator_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                  source_files={name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in paths.items()},
                  structure_preserved=True, binary_tail_preserved=True,
                  semantic_summary=semantics)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2), encoding='utf8')
    print(json.dumps({'materials': len(report['materials']), 'bound_channels': sum(len(v['bound']) for v in report['materials']),
                      'structure_preserved': True, 'binary_tail_preserved': True}))


if __name__ == '__main__': main()
