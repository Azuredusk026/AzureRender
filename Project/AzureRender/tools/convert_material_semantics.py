"""Describe material channels and restore Azure metadata through stable slots."""
import argparse
import copy
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct
import math
import uuid


def slots(document):
    materials = document.get('materials', [])
    names = Counter(material.get('name', '') for material in materials)
    identities = []
    for index, material in enumerate(materials):
        name = material.get('name', '')
        identity = material.get('extras', {}).get('azureSlotId', ('name:' + name) if name and names[name] == 1 else 'index:' + str(index))
        if not isinstance(identity, str) or not identity:
            raise ValueError('Invalid material slot identity')
        identities.append(identity)
    if len(set(identities)) != len(identities):
        raise ValueError('Duplicate material slot identity')
    return identities


def describe(document, expected_count=None):
    materials = document.get('materials', [])
    if expected_count is not None and len(materials) != expected_count:
        raise ValueError('Material count differs from expected count')
    result = {'schemaVersion': 1, 'materials': [], 'textures': copy.deepcopy(document.get('textures', [])),
              'images': copy.deepcopy(document.get('images', [])),
              'shaderEquivalenceVerified': False}
    for index, (material, identity) in enumerate(zip(materials, slots(document))):
        pbr = material.get('pbrMetallicRoughness', {})
        channels = {}
        specs = [('baseColor', pbr.get('baseColorTexture'), 'rgb', 'srgb'),
                 ('normal', material.get('normalTexture'), 'rgb', 'linear'),
                 ('roughness', pbr.get('metallicRoughnessTexture'), 'g', 'linear'),
                 ('metallic', pbr.get('metallicRoughnessTexture'), 'b', 'linear'),
                 ('packedAo', pbr.get('metallicRoughnessTexture'), 'r', 'linear'),
                 ('emissive', material.get('emissiveTexture'), 'rgb', 'srgb'),
                 ('occlusion_source', material.get('occlusionTexture'), 'r', 'linear')]
        losses = []
        for semantic, info, channel, color in specs:
            if info is None:
                continue
            texture = info.get('index')
            if type(texture) is not int or not 0 <= texture < len(result['textures']):
                raise ValueError('Invalid texture reference for ' + semantic)
            image = result['textures'][texture].get('source')
            if type(image) is not int or not 0 <= image < len(result['images']):
                raise ValueError('Invalid image source for texture')
            channels[semantic] = {'texture': texture, 'channel': channel, 'colorSpace': color}
            if info.get('texCoord', 0) != 0 or info.get('extensions'):
                losses.append({'code': 'uv_mapping_requires_review', 'semantic': semantic})
        if 'occlusion_source' in channels:
            losses.append({'code': 'occlusion_is_style_mask', 'detail': 'AssetMaterial routes occlusionTexture.r to styleMask; physical AO consumes packedMaterial.r only with surface-ao.'})
        extras = copy.deepcopy(material.get('extras', {}))
        profile = extras.get('azureRenderMaterial', {})
        if profile.get('faceSdf') or any(key.startswith('afterglow') for key in extras):
            losses.append({'code': 'specialized_profile_requires_gltf', 'detail': 'Keep embedded Azure metadata; azurematerial builtin overrides cannot express every specialized field.'})
        if material.get('alphaMode', 'OPAQUE') != 'OPAQUE':
            losses.append({'code': 'alpha_requires_gltf', 'detail': 'Preserve glTF alpha mode and cutoff; builtin material documents have no alpha mode field.'})
        result['materials'].append({'index': index, 'slot': identity, 'name': material.get('name', ''),
            'shaderIdentity': extras.get('shader', extras.get('shaderName', 'unknown')),
            'channels': channels, 'sourceMaterial': copy.deepcopy(material), 'losses': losses,
            'pipelineDependencies': {key: 'unknown' for key in ('screenDepth', 'sceneColor', 'multiplePasses')}})
    return result


def restore(document, mapping):
    if mapping.get('schemaVersion') != 1:
        raise ValueError('Unsupported mapping version')
    # Custom metadata can contain texture indices. Require their identity layout
    # to remain exact instead of silently assigning a different texture.
    if document.get('textures', []) != mapping.get('textures', []) or document.get('images', []) != mapping.get('images', []):
        raise ValueError('Texture identity layout changed; explicit texture remapping is required')
    entries = {entry['slot']: entry for entry in mapping['materials']}
    identities = slots(document)
    if len(entries) != len(mapping['materials']) or set(identities) != set(entries):
        raise ValueError('Material slot set changed')
    restored = copy.deepcopy(document)
    for material, identity in zip(restored['materials'], identities):
        original = entries[identity]['sourceMaterial']
        # Full material restoration also preserves alpha, extensions and factors.
        material.clear()
        material.update(copy.deepcopy(original))
        material.setdefault('extras', {})['azureSlotId'] = identity
    return restored


def restore_with_source(document, mapping, source):
    """Restore stable material slots with the exact source image payloads."""
    import base64
    if hashlib.sha256(source.read_bytes()).hexdigest()!=mapping.get('source',{}).get('sha256'):
        raise ValueError('Source fingerprint differs from material mapping')
    original, tail=read_asset(source)
    if describe(original)['materials']!=mapping['materials']:
        raise ValueError('Source material mapping differs from source')
    result=copy.deepcopy(document);images=[]
    total=0
    for image in original.get('images',[]):
        image=copy.deepcopy(image)
        if 'uri' in image:
            uri=image['uri']
            if uri.startswith('data:'):
                payload=base64.b64decode(uri.split(',',1)[1],validate=True)
                mime=uri.split(';',1)[0][5:]
            else:
                image_path=(source.parent/uri).resolve()
                payload=image_path.read_bytes()
                mime=image.get('mimeType','image/png' if image_path.suffix.lower()=='.png' else 'image/jpeg')
        else:
            view=original['bufferViews'][image.pop('bufferView')]
            if view.get('buffer',0)!=0 or tail is None or len(tail)<8 or struct.unpack_from('<I',tail,4)[0]!=0x004e4942:
                raise ValueError('Unsupported source image buffer')
            begin=8+view.get('byteOffset',0);end=begin+view['byteLength']
            if begin<8 or end>len(tail):raise ValueError('Invalid source image range')
            payload=tail[begin:end];mime=image['mimeType']
        total+=len(payload)
        if total>256*1024*1024:raise ValueError('Source images exceed 256 MiB budget')
        image['uri']='data:'+mime+';base64,'+base64.b64encode(payload).decode()
        images.append(image)
    result['images']=images;result['textures']=copy.deepcopy(original.get('textures',[]))
    result['samplers']=copy.deepcopy(original.get('samplers',[]))
    for key in ('extensionsUsed','extensionsRequired'):
        values=sorted(set(result.get(key,[]))|set(original.get(key,[])))
        if values:result[key]=values
    rebased=copy.deepcopy(mapping);rebased['images']=images
    return restore(result,rebased)


def material_document(material, texture_uuids):
    """Convert the exact supported PBR subset; reject lossy overrides."""
    extras = material.get('extras', {})
    if extras.get('azureRenderMaterial') or any(key.startswith('afterglow') for key in extras):
        raise ValueError('Keep specialized material metadata in glTF')
    if material.get('alphaMode', 'OPAQUE') != 'OPAQUE' or material.get('extensions') or material.get('occlusionTexture'):
        raise ValueError('Material override cannot express source alpha/extensions/occlusion')
    pbr = material.get('pbrMetallicRoughness', {})
    emission = material.get('emissiveFactor', [0, 0, 0])
    if len(emission) != 3 or len(set(emission)) != 1:
        raise ValueError('Emissive color factor requires texture baking')
    params = {'baseColor': pbr.get('baseColorFactor', [1, 1, 1, 1]),
        'roughness': pbr.get('roughnessFactor', 1), 'metallic': pbr.get('metallicFactor', 1),
        'emissiveStrength': emission[0], 'transmission': 0, 'ior': 1.5, 'thickness': 0,
        'attenuationDistance': 0, 'attenuationColor': [1, 1, 1, 1], 'doubleSided': material.get('doubleSided', False)}
    for key, maximum in [('roughness', 1), ('metallic', 1), ('emissiveStrength', 32)]:
        value = params[key]
        if type(value) not in (int, float) or not math.isfinite(value) or not 0 <= value <= maximum:
            raise ValueError('Invalid material parameter: ' + key)
    if len(params['baseColor']) != 4 or any(type(v) not in (int, float) or not math.isfinite(v) or not 0 <= v <= 1 for v in params['baseColor']):
        raise ValueError('Invalid baseColor')
    if type(params['doubleSided']) is not bool:
        raise ValueError('Invalid doubleSided')
    textures = {}
    for slot, usage, info in [('albedo', 'albedo', pbr.get('baseColorTexture')),
                              ('normal', 'normal', material.get('normalTexture')),
                              ('metallicRoughness', 'roughness', pbr.get('metallicRoughnessTexture')),
                              ('emissive', 'emissive', material.get('emissiveTexture'))]:
        if info is None:
            continue
        if info.get('texCoord', 0) != 0 or info.get('extensions') or info.get('scale', 1) != 1:
            raise ValueError('Texture mapping requires baking')
        asset = texture_uuids.get(str(info['index']))
        try:
            if not isinstance(asset, str) or str(uuid.UUID(asset)) != asset.lower():
                raise ValueError()
        except (ValueError, AttributeError):
            raise ValueError('Texture requires a registered asset UUID') from None
        textures[slot] = {'asset': asset, 'usage': usage}
    return {'schemaVersion': 1, 'typeId': 'azure.pbr', 'parameters': params, 'textures': textures}


def complete_material_document(material, model, image_uuids):
    """Preserve a full glTF material through the public profile document."""
    surface_material = copy.deepcopy(material)
    selected = []

    def reference(index):
        if type(index) is float:
            if not math.isfinite(index) or index != math.trunc(index):
                raise ValueError('Invalid source texture index')
            index = int(index)
        if type(index) is not int or not 0 <= index < len(model.get('textures', [])):
            raise ValueError('Invalid source texture index')
        if index not in selected:
            selected.append(index)
        return selected.index(index)

    def walk(value):
        if isinstance(value, dict):
            for key, child in value.items():
                if (key.endswith('Texture') or key == 'texture') and type(child) in (int, float):
                    value[key] = reference(child)
                elif key.endswith('Texture') and isinstance(child, dict) and 'index' in child:
                    child['index'] = reference(child['index'])
                else:
                    walk(child)
        elif isinstance(value, list):
            for child in value:
                walk(child)

    walk(surface_material)
    images, textures, bindings = [], [], {}
    for source_index in selected:
        texture = copy.deepcopy(model['textures'][source_index])
        image = texture['source']
        if image not in images:
            identity = image_uuids[str(image)]
            if str(uuid.UUID(identity)) != identity.lower():
                raise ValueError('Profile images require UUIDs')
            bindings[str(len(images))] = {'asset': identity, 'usage': 'linear'}
            images.append(image)
        texture['source'] = images.index(image)
        textures.append(texture)
    surface = {'asset': {'version': '2.0'}, 'materials': [surface_material],
               'textures': textures, 'samplers': copy.deepcopy(model.get('samplers', [])),
               'nodes': [{'name': node.get('name', '')} for node in model.get('nodes', [])]}
    return {'schemaVersion': 3, 'typeId': 'azure.profile', 'parameters': {},
            'textures': bindings, 'surface': surface}


def read_asset(path):
    raw = path.read_bytes()
    if path.suffix.lower() != '.glb':
        return json.loads(raw), None
    if len(raw) < 20 or struct.unpack_from('<4sII', raw) != (b'glTF', 2, len(raw)):
        raise ValueError('Invalid GLB header')
    size, kind = struct.unpack_from('<II', raw, 12)
    if kind != 0x4e4f534a or 20 + size > len(raw):
        raise ValueError('Invalid GLB JSON chunk')
    return json.loads(raw[20:20 + size]), raw[20 + size:]


def write_asset(path, document, tail):
    encoded = json.dumps(document, ensure_ascii=False, separators=(',', ':')).encode('utf-8')
    if tail is not None:
        encoded += b' ' * (-len(encoded) % 4)
        encoded = struct.pack('<4sIII', b'glTF', 2, 20 + len(encoded) + len(tail), len(encoded)) + struct.pack('<I', 0x4e4f534a) + encoded + tail
    path.write_bytes(encoded)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path, help='Mapping report JSON')
    parser.add_argument('--expected-count', type=int)
    parser.add_argument('--restore-map', type=Path)
    parser.add_argument('--restore-source', type=Path, help='Fingerprint-verified original asset for exact texture recovery')
    parser.add_argument('--candidate', type=Path)
    parser.add_argument('--documents-dir', type=Path, help='Export supported PBR material documents')
    parser.add_argument('--texture-uuids', type=Path, help='JSON mapping glTF texture indices to registered image UUIDs')
    args = parser.parse_args()
    protected = {p.resolve() for p in (args.input, args.restore_map, args.restore_source, args.texture_uuids) if p}
    outputs = [p.resolve() for p in (args.output, args.candidate) if p]
    if any(path in protected for path in outputs) or len(set(outputs)) != len(outputs):
        raise ValueError('Output paths must be distinct from read-only inputs and each other')
    document, tail = read_asset(args.input)
    if args.restore_source and not args.restore_map:raise ValueError('Source recovery requires a material map')
    if args.restore_map:
        mapping=json.loads(args.restore_map.read_text(encoding='utf-8'))
        document=restore_with_source(document,mapping,args.restore_source) if args.restore_source else restore(document,mapping)
    report = describe(document, args.expected_count)
    report['source'] = {'path': str(args.input.resolve()), 'sha256': hashlib.sha256(args.input.read_bytes()).hexdigest()}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if args.documents_dir:
        identifiers = json.loads(args.texture_uuids.read_text(encoding='utf-8')) if args.texture_uuids else {}
        candidates = [(entry, material_document(entry['sourceMaterial'], identifiers)) for entry in report['materials']]
        destinations = [(args.documents_dir / ('material-' + str(entry['index']) + '.azurematerial')).resolve() for entry, _ in candidates]
        if any(path in protected or path in outputs for path in destinations):
            raise ValueError('Material document output conflicts with an input or report')
        args.documents_dir.mkdir(parents=True, exist_ok=True)
        for entry, data in candidates:
            destination = args.documents_dir / ('material-' + str(entry['index']) + '.azurematerial')
            destination.write_text(json.dumps(data, indent=2), encoding='utf-8')
    if args.candidate:
        if args.candidate.resolve() == args.input.resolve():
            raise ValueError('Candidate must differ from read-only source')
        if (tail is None) != (args.candidate.suffix.lower() != '.glb'):
            raise ValueError('Candidate container must match source')
        args.candidate.parent.mkdir(parents=True, exist_ok=True)
        write_asset(args.candidate, restore(document, report), tail)
        report['candidate'] = {'path': str(args.candidate.resolve()), 'sha256': hashlib.sha256(args.candidate.read_bytes()).hexdigest()}
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')


if __name__ == '__main__':
    main()
