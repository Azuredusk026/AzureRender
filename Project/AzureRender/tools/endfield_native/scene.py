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
    parser.add_argument('--output', type=Path, required=True)
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
    if not all(math.isfinite(v) for v in bounds) or any(bounds[i] >= bounds[i+1] for i in (0, 2, 4)):
        raise ValueError('Invalid selection bounds')
    pattern = re.compile(rf'^Data/Streaming/PC/{re.escape(args.scene)}/Streaming/InitChunkData_(?:Global|-?\d+_-?\d+)_\d+_\d+\.bytes$')
    group = resources.load_group(args.vfs_root, 'C3442D43')
    entries = sorted((e for e in group.iter_files() if pattern.fullmatch(e.file_name)), key=lambda e:e.file_name)
    if not entries:
        raise ValueError('Scene has no InitChunkData')
    paths = build_path_id_index(args.path_hash)
    chunks, records, renderers = [], [], {}
    for entry in entries:
        data = resources.read_virtual_file(args.vfs_root, 'C3442D43', entry.file_name)
        validate_raw_header(data)
        rows = list(iter_entities(data))
        columns = component_columns(data)
        chunks.append(dict(path=entry.file_name, bytes=len(data), sha256=hashlib.sha256(data).hexdigest(), instances=len(rows)))
        for row in rows:
            row['matrix'] = list(struct.unpack('<16f', row.pop('matrix_blob')))
            row['source'] = entry.file_name
            records.append(row)
            if not all(bounds[i*2] <= row[k] < bounds[i*2+1] for i,k in enumerate(('x','y','z'))):
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
    result = dict(format='AzureEndfieldRawSceneInventory/1', scene=args.scene,
                  layout='raw_flatbuffer_game_1.0.14', bounds=bounds, chunks=chunks,
                  instances=unique, duplicates=duplicates, renderers=list(renderers.values()),
                  path_hash_sha256=hashlib.sha256(args.path_hash.read_bytes()).hexdigest(),
                  status='source_references_verified_visual_acceptance_pending')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('x', encoding='utf8') as out:
        json.dump(result, out, ensure_ascii=False, indent=2, allow_nan=False)
    print(json.dumps(dict(chunks=len(chunks), instances=len(unique), duplicates=duplicates,
                          selected=len(renderers), pairs=sum(len(v['pairs']) for v in renderers.values()))))


if __name__ == '__main__':
    main()
