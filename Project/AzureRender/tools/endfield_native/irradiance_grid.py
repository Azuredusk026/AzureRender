"""Export an explicitly selected native GI file as a portable AZIV0001 grid.

This sampled reconstruction is a candidate field, not an equivalent native
clipmap. Activation is unverified and candidates stay disabled by default.
"""
from __future__ import annotations
import argparse
from collections import Counter
import hashlib
import json
import math
from pathlib import Path
import struct

from .irradiance import parse_index, parse_sparse_tile, world_address

MAX_SAMPLES = 1 << 20


def export_grid(directory: Path, output: Path, origin: tuple[float, float, float],
                spacing: float, dimensions: tuple[int, int, int], file_index: int,
                lods: list[int]) -> dict:
    if (len(origin) != 3 or not all(math.isfinite(x) for x in origin)
            or not math.isfinite(spacing) or spacing <= 0 or len(dimensions) != 3
            or any(type(d) is not int or d <= 0 for d in dimensions)
            or math.prod(dimensions) > MAX_SAMPLES or not lods
            or any(type(l) is not int or not 0 <= l <= 2 for l in lods)
            or lods != sorted(set(lods))):
        raise ValueError('Invalid irradiance grid or fine-to-coarse LOD selection')
    endpoint = tuple(x + spacing * (d-1) for x, d in zip(origin, dimensions))
    world_address(origin, 0); world_address(endpoint, 0)
    index = parse_index((directory/'index.bytes').read_bytes())
    if type(file_index) is not int or not 0 <= file_index < len(index['files']):
        raise ValueError('Irradiance file selection is outside index')
    name = index['files'][file_index]
    source = (directory/name).read_bytes()
    rows = {}
    for row in index['tiles']:
        if row['file_index'] == file_index:
            key = tuple(row['coordinate'])
            if key in rows: raise ValueError('Ambiguous source irradiance tile')
            rows[key] = row
    cache, tile_counts, lod_counts = {}, Counter(), Counter()
    samples, valid = [], 0
    for z in range(dimensions[2]):
        for y in range(dimensions[1]):
            for x in range(dimensions[0]):
                position = tuple(o + spacing*a for o, a in zip(origin, (x,y,z)))
                found = None
                for lod in lods:
                    address = world_address(position, lod)
                    row = rows.get(tuple(address['tile']))
                    if row is None: continue
                    i = row['index']
                    if i not in cache:
                        begin = row['file_offset']
                        cache[i] = parse_sparse_tile(source[begin:begin+row['bytes']], row['header_bytes'])
                    found = cache[i].sample(tuple(address['brick']), address['occupancy_bit'])
                    if found:
                        tile_counts[str(i)] += 1; lod_counts[str(lod)] += 1; valid += 1
                        break
                if found:
                    values = [v for color in found['directional_coefficients'] for v in color] + [1,0,0,0]
                else: values = [0]*16
                samples.append(struct.pack('<16f', *values))
    payload = struct.pack('<8s4f3I', b'AZIV0001', *origin, spacing, *dimensions) + b''.join(samples)
    report = dict(schema='NativeIrradianceGridExport/1', format='AZIV0001', origin=list(origin),
                  spacing=spacing, dimensions=list(dimensions), file_index=file_index,
                  source_file=name, source_index_sha256=index['source_sha256'],
                  source_file_sha256=hashlib.sha256(source).hexdigest(),
                  lod_order=lods, selection='first occupied sample in explicit fine-to-coarse order',
                  activation_verified=False, enabled_by_default=False,
                  equivalence='sampled candidate field; native clipmap interpolation and transitions differ',
                  valid_samples=valid, missing_samples=len(samples)-valid,
                  samples_per_lod=dict(lod_counts), samples_per_tile=dict(tile_counts),
                  tile_cache_misses=len(cache), output_sha256=hashlib.sha256(payload).hexdigest(), bytes=len(payload))
    output.write_bytes(payload)
    output.with_suffix('.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    return report


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--origin',type=float,nargs=3,required=True)
    parser.add_argument('--spacing',type=float,required=True)
    parser.add_argument('--dimensions',type=int,nargs=3,required=True)
    parser.add_argument('--file-index',type=int,required=True)
    parser.add_argument('--lod',type=int,nargs='+',required=True)
    args=parser.parse_args()
    result=export_grid(args.directory,args.output,tuple(args.origin),args.spacing,
                       tuple(args.dimensions),args.file_index,args.lod)
    print(json.dumps({k:result[k] for k in ('valid_samples','missing_samples','tile_cache_misses','bytes')}))
