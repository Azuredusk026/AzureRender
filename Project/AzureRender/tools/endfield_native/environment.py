"""Preserve typed environment metadata and resolve serialized-file references.

Input is the pinned EndfieldSceneProbe text dump. Field values are observations;
runtime activation, units and shader interpretation need separate evidence.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re

from endfield_native.render_inventory import build_inventory


def parse_metadata(text):
    fields, parents, indices = {}, [], {}
    for line_number, line in enumerate(text.splitlines(), 1):
        if not line.strip():
            continue
        depth = len(line) - len(line.lstrip('\t'))
        indices = {d: index for d, index in indices.items() if d <= depth}
        if re.fullmatch(r'\[\d+\]', line.strip()):
            indices[depth] = line.strip()
            continue
        declaration, separator, raw = line.strip().partition(' = ')
        if ' ' not in declaration:
            # Array indices are preserved as structural paths.
            field_type, name = 'index', declaration
        else:
            field_type, name = declaration.split(' ', 1)
        if declaration.startswith('unsigned int '):
            field_type, name = 'unsigned int', declaration[len('unsigned int '):]
        if name == 'data' and depth in indices:
            name += indices[depth]
        if depth > len(parents):
            raise ValueError(f'invalid metadata indentation at {line_number}')
        parents = parents[:depth]
        parents.append(name)
        if not separator:
            continue
        path = '/'.join(parents[1:])
        if path in fields:
            raise ValueError(f'duplicate field: {path}')
        if field_type in ('float', 'double'):
            value = float(raw)
            if not math.isfinite(value):
                raise ValueError(f'non-finite field: {path}')
        elif field_type in ('int', 'unsigned int', 'SInt64', 'UInt64', 'UInt8', 'UInt16', 'SInt8', 'SInt16'):
            value = int(raw)
        elif field_type == 'string':
            value = json.loads(raw)
        else:
            value = raw
        fields[path] = {'type': field_type, 'value': value, 'line': line_number}
    return fields


def resolve_reference(source, file_id, path_id, available):
    if type(file_id) is not int or type(path_id) is not int or file_id < 0:
        raise ValueError('invalid reference identity')
    if path_id == 0:
        return None
    if file_id > len(source['ExternalFiles']):
        raise ValueError('external reference index out of bounds')
    cab = source['Name'] if file_id == 0 else source['ExternalFiles'][file_id - 1]
    if cab not in available:
        raise ValueError(f'missing dependency: {cab}')
    return cab, path_id


def inventory_probe(probe, game_version):
    manifest = json.loads((probe / 'scene_manifest.json').read_text(encoding='utf-8-sig'))
    if manifest.get('Format') != 'EndfieldSceneProbe/1':
        raise ValueError('unsupported probe format')
    sources = {}
    available = {x['Name'] for x in manifest['SerializedFiles']}
    for source in manifest['SerializedFiles']:
        stem = Path(source['OriginalPath']).stem
        if stem in sources:
            raise ValueError(f'ambiguous bundle stem: {stem}')
        sources[stem] = source
    objects, unresolved = [], []
    for path in sorted((probe / 'metadata').glob('*.txt')):
        match = re.fullmatch(r'([0-9a-f]+)_(-?\d+)_.+\.txt', path.name)
        if match is None:
            raise ValueError(f'unsupported metadata filename: {path.name}')
        source = sources[match[1]]
        fields = parse_metadata(path.read_text(encoding='utf-8-sig'))
        refs = {}
        for key, field in fields.items():
            if not key.endswith('/m_PathID'):
                continue
            parent = key.rsplit('/', 1)[0]
            try:
                refs[parent] = resolve_reference(source, fields[parent + '/m_FileID']['value'],
                                                 field['value'], available)
            except ValueError as error:
                unresolved.append({'object': [source['Name'], int(match[2])],
                                   'field': parent, 'error': str(error)})
        objects.append({'identity': [source['Name'], int(match[2])],
            'bundle': source['OriginalPath'], 'unity_version': source['UnityVersion'],
            'activation': 'unverified', 'evidence': 'serialized_field_observation',
            'units': 'unverified_source_values', 'coordinate_system': 'source_unity',
            'metadata_path': str(path.resolve()),
            'metadata_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
            'name': fields.get('m_Name', {}).get('value'), 'fields': fields, 'references': refs})
    return build_inventory(game_version, objects, unresolved)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--game-version', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = inventory_probe(args.probe, args.game_version)
    with args.output.open('x', encoding='utf-8') as stream:
        json.dump(result, stream, indent=2, ensure_ascii=False)
    print(json.dumps({'objects': len(result['objects']), 'status': result['status'],
                      'unresolved_dependencies': result['unresolved_dependencies']}))


if __name__ == '__main__':
    main()
