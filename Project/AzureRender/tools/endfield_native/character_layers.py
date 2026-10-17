"""Recover explicit additional renderer draws, retaining unsupported native passes."""
import argparse
import base64
import copy
import hashlib
import json
from pathlib import Path
import sys


def identity(row):
    return row['source'], row['path_id']


def restore_layers(document, audit, sources, texture_payloads):
    result = copy.deepcopy(document)
    report = {'restored': [], 'unresolved': [], 'shader_equivalence_verified': False}
    by_id = {identity(s['object']): s for s in sources}
    if len(by_id) != len(sources):
        raise ValueError('Ambiguous source material identity')
    details = {identity(s['object']): s for s in audit.get('materials', [])}
    transforms = {s['path']: s for s in audit['transforms']}
    for mesh in result['meshes']:
        candidates = [r for r in audit['renderers'] if r['path'].split('/')[-1] == mesh.get('name')]
        if len(candidates) > 1:
            raise ValueError('Ambiguous renderer name: ' + mesh['name'])
        if not candidates:
            continue
        renderer = candidates[0]
        path = renderer['path']; active = renderer['enabled']
        while path:
            if path not in transforms:
                raise ValueError('Missing source ancestor: ' + path)
            active = active and transforms[path]['game_object_active']
            path = path.rpartition('/')[0]
        if not active:
            continue
        count = renderer['submesh_count']
        if len(renderer['materials']) <= count:
            continue
        if count <= 0 or len(mesh['primitives']) != count:
            raise ValueError('Source/converted submesh count mismatch')
        last = copy.deepcopy(mesh['primitives'][-1])
        for slot in renderer['materials'][count:]:
            key = identity(slot['resolved']); source = by_id[key]
            raw = source['data']; name = source['object']['name']
            row = {'renderer': renderer['path'], 'material': source['object']}
            def unresolved(reason):
                report['unresolved'].append(dict(row, reason=reason, source_properties=raw))
            if 'ForwardOnly' in raw.get('disabledShaderPasses', []):
                unresolved('source_forward_pass_disabled'); continue
            floats = {v['Key']: v['Value'] for v in raw['m_SavedProperties'].get('m_Floats', [])}
            if not all(floats.get(k) == v for k, v in {'_SurfaceType': 1, '_SrcBlend': 5, '_DstBlend': 10, '_AlphaClip': 0}.items()):
                unresolved('unsupported_native_blend_contract'); continue
            material = copy.deepcopy(result['materials'][last['material']])
            material['name'] = name
            material['alphaMode'] = 'BLEND'
            material.pop('alphaCutoff', None)
            material['extras'] = {'nativeAdditionalDraw': {'source': source['object'], 'properties': raw,
                'shaderEquivalenceVerified': False, 'limitations': ['native_stencil_depth_and_specialized_shader_require_engine_support']}}
            properties = raw['m_SavedProperties']
            colors = {v['Key']: v['Value'] for v in properties.get('m_Colors', [])}
            pbr = material.setdefault('pbrMetallicRoughness', {})
            if '_BaseColor' in colors:
                c = colors['_BaseColor']; pbr['baseColorFactor'] = [c[x] for x in ('r', 'g', 'b', 'a')]
            binding = next((v for v in details.get(key, {}).get('textures', []) if v['slot'] == '_BaseMap'), None)
            if binding and binding['reference']['resolved']:
                tex_key = identity(binding['reference']['resolved'])
                if tex_key not in texture_payloads:
                    raise ValueError('Missing source BaseMap payload')
                payload = texture_payloads[tex_key]
                images = result.setdefault('images', []); textures = result.setdefault('textures', [])
                images.append({'name': binding['reference']['resolved']['name'], 'uri': 'data:image/png;base64,' + base64.b64encode(payload).decode('ascii')})
                textures.append({'source': len(images) - 1})
                info = {'index': len(textures) - 1}
                if binding['scale'] != [1, 1] or binding['offset'] != [0, 0]:
                    info['extensions'] = {'KHR_texture_transform': {'scale': binding['scale'], 'offset': binding['offset']}}
                    used = result.setdefault('extensionsUsed', [])
                    if 'KHR_texture_transform' not in used: used.append('KHR_texture_transform')
                pbr['baseColorTexture'] = info
            primitive = copy.deepcopy(last); primitive['material'] = len(result['materials'])
            result['materials'].append(material); mesh['primitives'].append(primitive)
            report['restored'].append(dict(row, mode='additional_draw_preview', source_properties=raw))
    return result, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('input', 'source-root', 'output', 'report'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists() or args.report.exists() or args.output.resolve() == args.report.resolve():
        raise ValueError('Outputs must be distinct new files')
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from convert_material_semantics import read_asset, write_asset
    def read(name): return json.loads((args.source_root / name).read_text(encoding='utf8'))
    payloads = {}
    for row in read('source_texture_artifacts.json'):
        payload = (args.source_root / row['png']['path']).read_bytes()
        if hashlib.sha256(payload).hexdigest().lower() != row['png']['sha256'].lower():
            raise ValueError('Source texture digest mismatch')
        payloads[identity(row['object'])] = payload
    document, tail = read_asset(args.input)
    result, report = restore_layers(document, read('source_audit.json'), read('material_sources.json'), payloads)
    write_asset(args.output, result, tail)
    report['input_sha256'] = hashlib.sha256(args.input.read_bytes()).hexdigest()
    report['output_sha256'] = hashlib.sha256(args.output.read_bytes()).hexdigest()
    report['animation_preserved'] = document.get('animations') == result.get('animations')
    report['skin_preserved'] = document.get('skins') == result.get('skins')
    args.report.write_text(json.dumps(report, indent=2), encoding='utf8')
    print(json.dumps({'restored': len(report['restored']), 'unresolved': len(report['unresolved'])}))


if __name__ == '__main__': main()
