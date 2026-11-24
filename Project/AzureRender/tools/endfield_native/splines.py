"""Read source spline descriptors without assuming native simulation semantics."""
import argparse
import hashlib
import json
from pathlib import Path
import re

from endfield_native.environment import parse_metadata, resolve_reference


def decode_descriptor(text, source, available):
    if '\tSplineObjectsDesc desc' not in text.splitlines():
        raise ValueError('missing spline descriptor')
    fields = parse_metadata(text)
    values = {key: field['value'] for key, field in fields.items()}
    try:
        name = values['desc/name']
        count = values['knotData/size']
        if not isinstance(name, str) or type(count) is not int or not 0 < count <= 65536:
            raise ValueError('invalid spline name or segment count')
        segments = []
        for index in range(count):
            prefix = f'knotData/size/data[{index}]'
            points = [[values[f'{prefix}/p{p}/{axis}'] for axis in 'xyz'] for p in range(4)]
            length = values[prefix + '/length']
            if any(type(v) not in (int, float) for p in points for v in p) or type(length) not in (int, float) or length < 0:
                raise ValueError('invalid spline geometry')
            segments.append({'controls': points, 'stored_length': length,
                             'geometrically_degenerate': all(p == points[0] for p in points)})
        refs, unresolved = {}, []
        for key in values:
            if not key.endswith('/m_PathID'):
                continue
            parent = key.rsplit('/', 1)[0]
            try:
                refs[parent] = resolve_reference(source, values[parent + '/m_FileID'], values[key], available)
            except ValueError as error:
                unresolved.append({'field': parent, 'error': str(error)})
    except KeyError as error:
        raise ValueError(f'incomplete spline descriptor: {error}') from error
    return {'name': name, 'fields': fields, 'references': refs,
            'unresolved_dependencies': unresolved, 'segments': segments,
            'active_segment_count': None, 'runtime_semantics': 'unverified',
            'scene_placement': 'unverified'}


def inventory_splines(probe):
    manifest = json.loads((probe / 'scene_manifest.json').read_text(encoding='utf-8-sig'))
    if manifest.get('Format') != 'EndfieldSceneProbe/1':
        raise ValueError('unsupported probe format')
    sources = {}
    for source in manifest['SerializedFiles']:
        stem = Path(source['OriginalPath']).stem
        if stem in sources:
            raise ValueError(f'ambiguous bundle stem: {stem}')
        sources[stem] = source
    available = {s['Name'] for s in sources.values()}
    emitters = []
    for path in sorted((probe / 'metadata').glob('*.txt')):
        # Descriptors precede payload fields; avoid loading unrelated shader dumps.
        with path.open(encoding='utf-8-sig') as stream:
            header = stream.read(4096)
        if '\tSplineObjectsDesc desc' not in header.splitlines():
            continue
        match = re.fullmatch(r'([0-9a-f]+)_(-?\d+)_.+\.txt', path.name)
        if match is None or match[1] not in sources:
            raise ValueError(f'unsupported source identity: {path.name}')
        source = sources[match[1]]
        raw = path.read_bytes()
        row = decode_descriptor(raw.decode('utf-8-sig'), source, available)
        row.update(identity=[source['Name'], int(match[2])],
                   metadata_path=str(path.resolve()), metadata_sha256=hashlib.sha256(raw).hexdigest())
        emitters.append(row)
    return {'format': 'NativeSplineInventory/1', 'evidence': 'serialized_fields_only',
            'status': 'runtime_semantics_unverified', 'emitters': emitters}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = inventory_splines(args.probe)
    with args.output.open('x', encoding='utf-8') as stream:
        json.dump(result, stream, ensure_ascii=False, indent=2)
    print(json.dumps({'emitters': len(result['emitters']), 'status': result['status'],
                      'unresolved_dependencies': sum(len(e['unresolved_dependencies']) for e in result['emitters'])}))


if __name__ == '__main__':
    main()
