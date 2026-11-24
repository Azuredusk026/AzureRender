"""Append explicitly bound unlit surface primitives without changing source data."""
import argparse
import base64
import copy
import hashlib
import json
from pathlib import Path
import tempfile

from convert_material_semantics import read_asset, write_asset, slots


def fields(value, required, optional=()):
    if not isinstance(value, dict) or set(required)-value.keys() or value.keys()-set(required)-set(optional):
        raise ValueError('Invalid overlay configuration fields')


def index(value, count, label):
    if type(value) is not int or not 0 <= value < count:
        raise ValueError('Invalid overlay '+label)
    return value


def append_overlays(document, configuration):
    fields(configuration, ['schemaVersion', 'images', 'textures', 'samplers', 'overlays'])
    if type(configuration['schemaVersion']) is not int or configuration['schemaVersion'] != 1:
        raise ValueError('Unsupported overlay configuration version')
    for key in ['images', 'textures', 'samplers', 'overlays']:
        if not isinstance(configuration[key], list) or len(configuration[key]) > 64:
            raise ValueError('Invalid overlay table '+key)
    if not configuration['overlays']:
        raise ValueError('At least one overlay binding is required')
    out=copy.deepcopy(document)
    source_slots=slots(document)
    existing=set(source_slots)
    image_base=len(out.get('images', []))
    texture_base=len(out.get('textures', []))
    sampler_base=len(out.get('samplers', []))
    for image in configuration['images']:
        fields(image, ['uri'])
        uri=image['uri']
        if not isinstance(uri, str) or not uri.startswith('data:image/png;base64,'):
            raise ValueError('Overlay images require embedded PNG data')
        try:
            raw=base64.b64decode(uri.split(',', 1)[1], validate=True)
        except (ValueError, TypeError) as error:
            raise ValueError('Invalid overlay image encoding') from error
        if len(raw) > 16*1024*1024 or not raw.startswith(b'\x89PNG\r\n\x1a\n'):
            raise ValueError('Invalid overlay PNG data')
    for sampler in configuration['samplers']:
        fields(sampler, [], ['wrapS', 'wrapT', 'minFilter', 'magFilter'])
    for texture in configuration['textures']:
        fields(texture, ['source'], ['sampler'])
        index(texture['source'], len(configuration['images']), 'image index')
        if 'sampler' in texture:
            index(texture['sampler'], len(configuration['samplers']), 'sampler index')

    def remap(value, key):
        value[key]=texture_base+index(value[key], len(configuration['textures']), 'texture index')

    records=[]
    # Select only original primitives. A later overlay cannot consume an earlier one.
    original_meshes=copy.deepcopy(document.get('meshes', []))
    for binding in configuration['overlays']:
        fields(binding, ['sourceSlot', 'uvSet', 'material'])
        source=binding['sourceSlot']
        if not isinstance(source, str) or source not in source_slots:
            raise ValueError('Unknown overlay source slot')
        uv_set=index(binding['uvSet'], 8, 'UV set')
        material=copy.deepcopy(binding['material'])
        fields(material, ['name', 'alphaMode', 'extras'], ['doubleSided', 'pbrMetallicRoughness'])
        if not isinstance(material['name'], str) or not material['name'] or material['alphaMode'] != 'BLEND':
            raise ValueError('Overlay requires a named BLEND material')
        if 'doubleSided' in material and type(material['doubleSided']) is not bool:
            raise ValueError('Invalid overlay sidedness')
        fields(material['extras'], ['azureRenderMaterial'], ['azureSlotId'])
        identity=slots(dict(materials=[material]))[0]
        if identity in existing:
            raise ValueError('Duplicate overlay material slot')
        existing.add(identity)
        profile=material['extras']['azureRenderMaterial']
        if (not isinstance(profile, dict) or profile.get('schemaVersion') != 2
                or profile.get('class') not in ['generic', 'emissive']
                or sorted(profile.get('features', [])) != ['unlit-surface', 'unlit-texture-layers']):
            raise ValueError('Overlay requires a complete unlit texture-layer profile')
        surface=profile.get('surface', {})
        if surface.get('schemaVersion') != 2 or surface.get('lighting') != 'unlit':
            raise ValueError('Overlay requires a version-two unlit surface')
        layers=surface.get('layers')
        if not isinstance(layers, list) or not 1 <= len(layers) <= 2:
            raise ValueError('Invalid overlay layer count')
        for layer in layers:
            if not isinstance(layer, dict) or 'texture' not in layer:
                raise ValueError('Overlay layer requires a texture')
            remap(layer, 'texture')
        for key in ['coverageLayer', 'baseUvDistortion']:
            if key in surface:
                if not isinstance(surface[key], dict) or 'texture' not in surface[key]:
                    raise ValueError('Overlay sample requires a texture')
                remap(surface[key], 'texture')
        pbr=material.get('pbrMetallicRoughness', {})
        fields(pbr, [], ['baseColorFactor', 'baseColorTexture'])
        if 'baseColorTexture' in pbr:
            remap(pbr['baseColorTexture'], 'index')
        new_material=len(out['materials'])
        source_material=source_slots.index(source)
        count=0
        for mesh_index, mesh in enumerate(original_meshes):
            for primitive in mesh.get('primitives', []):
                if primitive.get('material') != source_material:
                    continue
                attributes=primitive.get('attributes', {})
                uv='TEXCOORD_'+str(uv_set)
                if uv not in attributes or primitive.get('mode', 4) != 4:
                    raise ValueError('Overlay source requires triangles and the declared UV set')
                if type(attributes[uv]) is not int or attributes[uv] < 0:
                    raise ValueError('Invalid overlay UV accessor')
                overlay=copy.deepcopy(primitive)
                overlay['material']=new_material
                overlay['attributes']['TEXCOORD_0']=attributes[uv]
                out['meshes'][mesh_index]['primitives'].append(overlay)
                count+=1
        if not count:
            raise ValueError('Overlay source slot has no geometry')
        out['materials'].append(material)
        records.append(dict(sourceSlot=source, outputSlot=identity, material=new_material,
                            uvSet=uv_set, primitives=count))
    out.setdefault('images', []).extend(copy.deepcopy(configuration['images']))
    out.setdefault('samplers', []).extend(copy.deepcopy(configuration['samplers']))
    for source in configuration['textures']:
        texture=copy.deepcopy(source)
        texture['source']+=image_base
        if 'sampler' in texture:
            texture['sampler']+=sampler_base
        out.setdefault('textures', []).append(texture)
    return out, records


def main():
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--config', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args=parser.parse_args()
    source=args.input.resolve();output=args.output.resolve();config=args.config.resolve()
    receipt=Path(str(output)+'.overlays.json')
    if (output in [source, config] or source.parent != output.parent
            or source.suffix != output.suffix or source.suffix.lower() not in ['.gltf', '.glb']
            or output.exists() or receipt.exists()):
        raise ValueError('Require new output and provenance beside the source model')
    if source.stat().st_size > 128*1024*1024 or config.stat().st_size > 24*1024*1024:
        raise ValueError('Overlay source exceeds the input byte budget')
    sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
    source_hash=sha(source);config_hash=sha(config)
    document, tail=read_asset(source)
    result, bindings=append_overlays(document, json.loads(config.read_text(encoding='utf-8')))
    with tempfile.TemporaryDirectory(prefix='azure-overlay-', dir=output.parent) as temporary:
        candidate=Path(temporary)/output.name
        write_asset(candidate, result, tail)
        provenance=dict(schemaVersion=1, generator='azure.material-overlays', generatorVersion=1,
                        sourceSha256=source_hash, configSha256=config_hash,
                        toolSha256=sha(Path(__file__)), outputSha256=sha(candidate), bindings=bindings,
                        binaryChunkPreserved=True)
        if sha(source)!=source_hash or sha(config)!=config_hash:
            raise ValueError('Overlay inputs changed during conversion')
        created=False;receipt_created=False
        try:
            with output.open('xb') as stream:
                created=True;stream.write(candidate.read_bytes())
            with receipt.open('x', encoding='utf-8') as stream:
                receipt_created=True
                stream.write(json.dumps(provenance, ensure_ascii=False, indent=2)+'\n')
        except Exception:
            if created:
                output.unlink()
            if receipt_created:
                receipt.unlink()
            raise
    print(json.dumps(dict(status='appended', bindings=bindings)))


if __name__ == '__main__':
    main()
