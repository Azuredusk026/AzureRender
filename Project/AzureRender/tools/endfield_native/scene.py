"""Inventory the observed uncompressed InitChunkData layout through pinned readers.

Invoke through run_upstream.py. This accepts the game 1.0.14 raw layout only;
resource hashes and runtime draw references remain authoritative, not names.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import struct


def _validate_bounds(bounds, *, request=False):
    if (not isinstance(bounds, (list, tuple)) or len(bounds) != 6
            or any(type(v) not in (int, float) or not math.isfinite(v) for v in bounds)
            or any(bounds[i] >= bounds[i+1] if request else bounds[i] > bounds[i+1]
                   for i in (0, 2, 4))):
        raise ValueError('Invalid selection bounds' if request else 'Invalid source world bounds')


def select_instance(row, bounds, *, policy='world-bounds'):
    """Conservatively select whole geometry intersecting a closed region.

    Missing source bounds use the finite origin with half-open membership.
    The explicit origin policy retains the legacy point-selection contract.
    Malformed source bounds are errors rather than an origin fallback.
    """
    _validate_bounds(bounds, request=True)
    if policy not in ('world-bounds', 'origin'):
        raise ValueError('Unsupported spatial selection policy')
    source = row.get('world_bounds')
    if source is not None:
        _validate_bounds(source)
    if policy == 'world-bounds' and source is not None:
        return all(source[i] <= bounds[i+1] and source[i+1] >= bounds[i] for i in (0, 2, 4))
    origin = [row.get(k) for k in ('x', 'y', 'z')]
    if any(type(v) not in (int, float) or not math.isfinite(v) for v in origin):
        raise ValueError('Invalid finite instance origin')
    return all(bounds[i*2] <= value < bounds[i*2+1] for i, value in enumerate(origin))


def validate_raw_header(data):
    if len(data) < 24:
        raise ValueError('Truncated raw InitChunkData')
    root = struct.unpack_from('<I', data)[0]
    if root < 24 or root + 40 > len(data):
        raise ValueError('Invalid raw InitChunkData root')
    vtable = root - struct.unpack_from('<i', data, root)[0]
    if vtable < 4 or vtable + 20 > len(data):
        raise ValueError('Invalid raw InitChunkData vtable')
    fields = struct.unpack_from('<HH8H', data, vtable)
    if fields != (20, 40, 4, 8, 16, 20, 24, 28, 32, 36):
        raise ValueError('Unsupported raw InitChunkData layout')


def decode_chunk_data(data, decompress=None):
    """Accept a validated raw chunk or a size-prefixed inverted-LZ4 chunk."""
    try:
        validate_raw_header(data)
        return data
    except ValueError:
        pass
    if len(data) < 5:
        raise ValueError('Truncated scene chunk envelope')
    expected = struct.unpack_from('<I', data)[0]
    if not 64 <= expected <= 256*1024*1024:
        raise ValueError('Scene chunk decompression size outside budget')
    if decompress is None:
        from extractor_core.lz4 import decompress_lz4_inv
        decompress = decompress_lz4_inv
    decoded = decompress(data[4:], expected)
    if len(decoded) != expected:
        raise ValueError('Scene chunk decoded length differs')
    validate_raw_header(decoded)
    return decoded


def merge_instances(records):
    unique = {}
    duplicates = 0
    for row in records:
        matrix = row['matrix']
        if (len(matrix) != 16 or not all(math.isfinite(v) for v in matrix)
                or any(abs(matrix[i]) > 1e-6 for i in (3, 7, 11))
                or abs(matrix[15] - 1) > 1e-6):
            raise ValueError('Invalid finite affine instance matrix')
        old = unique.get(row['entity_id'])
        if old is not None:
            if old['matrix'] != matrix or old['name'] != row['name']:
                raise ValueError(f"conflicting entity identity: {row['entity_id']}")
            duplicates += 1
        else:
            unique[row['entity_id']] = row
    return list(unique.values()), duplicates


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--vfs-root', type=Path, required=True)
    parser.add_argument('--scene', required=True)
    parser.add_argument('--path-hash', type=Path, required=True)
    parser.add_argument('--bounds', type=float, nargs=6, required=True,
                        metavar=('XMIN', 'XMAX', 'YMIN', 'YMAX', 'ZMIN', 'ZMAX'))
    parser.add_argument('--selection-policy', choices=('world-bounds', 'origin'), default='world-bounds',
                        help='Closed world-bounds overlap; missing bounds use half-open origin membership')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--source-chunk', action='append',
                        help='Exact VFS chunk path; repeat to select multiple sources')
    args = parser.parse_args()
    from extractor_core import resources
    from extractor_core.instances import iter_entities
    from extractor_core.scene_identity import validate_scene_id
    from extractor_core.projected_decals import build_path_id_index
    from extract_scene_renderer_materials import component_columns, decode_pairs, decode_draws
    validate_scene_id(args.scene)
    if args.output.exists() or args.output.resolve().is_relative_to(args.vfs_root.resolve()):
        raise ValueError('Output must be a new file outside the game')
    bounds = args.bounds
    _validate_bounds(bounds, request=True)
    pattern = re.compile(rf'^Data/Streaming/PC/{re.escape(args.scene)}/Streaming/InitChunkData_(?:Global|-?\d+_-?\d+)_\d+_\d+\.bytes$')
    group = resources.load_group(args.vfs_root, 'C3442D43')
    entries = sorted((e for e in group.iter_files() if pattern.fullmatch(e.file_name)), key=lambda e:e.file_name)
    if not entries:
        raise ValueError('Scene has no InitChunkData')
    from endfield_native.scene_sources import select_chunk_paths
    available_chunks = [e.file_name for e in entries]
    selected_chunks = select_chunk_paths(available_chunks, args.source_chunk)
    selected_set = set(selected_chunks)
    entries = [e for e in entries if e.file_name in selected_set]
    paths = build_path_id_index(args.path_hash)
    chunks, records, renderers = [], [], {}
    for entry in entries:
        encoded = resources.read_virtual_file(args.vfs_root, 'C3442D43', entry.file_name)
        data = decode_chunk_data(encoded)
        rows = list(iter_entities(data))
        columns = component_columns(data)
        chunks.append(dict(path=entry.file_name, bytes=len(data), sha256=hashlib.sha256(data).hexdigest(),
                           encoded_sha256=hashlib.sha256(encoded).hexdigest(), instances=len(rows)))
        for row in rows:
            row['matrix'] = list(struct.unpack('<16f', row.pop('matrix_blob')))
            row['source'] = entry.file_name
            records.append(row)
            if not select_instance(row, bounds, policy=args.selection_policy):
                continue
            pairs = []
            cs = columns.get(row['entity_id'], {})
            for kind, raw in cs.items():
                if 68 <= kind <= 74:
                    refs = decode_pairs(raw, paths)
                    draws = decode_draws(cs[kind-60], raw)
                    if len(refs) != len(draws):
                        raise ValueError('Draw and resource reference counts differ')
                    pairs.extend(dict(mesh=p[0], material=p[1], draw=d._asdict()) for p,d in zip(refs, draws))
            old = renderers.get(row['entity_id'])
            value = dict(instance=row, pairs=pairs)
            if old is not None and old['pairs'] != pairs:
                raise ValueError('Conflicting duplicate renderer references')
            renderers.setdefault(row['entity_id'], value)
    unique, duplicates = merge_instances(records)
    selected = list(renderers.values())
    selection = dict(policy=args.selection_policy,
                     source_chunks=dict(schemaVersion=1,
                         policy='all' if args.source_chunk is None else 'explicit',
                         selectedChunks=selected_chunks,
                         excludedChunks=[p for p in available_chunks if p not in selected_set]),
                     geometry_boundary='closed_overlap' if args.selection_policy == 'world-bounds' else 'half_open_origin',
                     missing_bounds='half_open_origin',
                     selected_without_world_bounds=sum(row['instance'].get('world_bounds') is None for row in selected),
                     selected_origin_outside=sum(not select_instance(row['instance'], bounds, policy='origin') for row in selected))
    result = dict(format='AzureEndfieldRawSceneInventory/1', scene=args.scene,
                  layout='validated_flatbuffer_root8', bounds=bounds, chunks=chunks,
                  instances=unique, duplicates=duplicates, renderers=selected, selection=selection,
                  path_hash_sha256=hashlib.sha256(args.path_hash.read_bytes()).hexdigest(),
                  status='source_references_verified_visual_acceptance_pending')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('x', encoding='utf8') as out:
        json.dump(result, out, ensure_ascii=False, indent=2, allow_nan=False)
    print(json.dumps(dict(chunks=len(chunks), instances=len(unique), duplicates=duplicates,
                          selected=len(renderers), pairs=sum(len(v['pairs']) for v in renderers.values()))))


if __name__ == '__main__':
    main()
