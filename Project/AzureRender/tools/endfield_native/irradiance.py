"""Read the observed native v3 irradiance index and sparse source payloads.

Evidence: native IrradianceVolumeAtlasBakeV3CS DXBC region upload and
IrradianceVolumeIndirectUpdateV2CS spatial-hash lookup. V3 file-range,
occupancy and hash identities were checked on both source scene branches.
V3 clipmap update establishes world lattice selection and directional atlas
byte normalization. The deferred pixel consumer establishes signed linear
coefficients and a clamped single-sample volume contribution. Scenario selection,
region overrides, default SH and optional visibility processing are separate.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
import math
from pathlib import Path
import struct


class Reader:
    def __init__(self, data: bytes):
        self.data, self.offset = data, 0

    def take(self, size: int) -> bytes:
        if size < 0 or self.offset + size > len(self.data):
            raise ValueError('Truncated irradiance data')
        result = self.data[self.offset:self.offset + size]
        self.offset += size
        return result

    def uint(self) -> int:
        return struct.unpack('<I', self.take(4))[0]

    def count(self, stride: int) -> int:
        count = self.uint()
        if count > (len(self.data) - self.offset) // stride:
            raise ValueError('Irradiance count exceeds source size')
        return count

    def string(self) -> str:
        size = self.uint()
        if size % 2:
            raise ValueError('Invalid UTF-16 irradiance string length')
        try:
            return self.take(size).decode('utf-16le')
        except UnicodeError as exc:
            raise ValueError('Invalid irradiance string') from exc


def parse_index(data: bytes) -> dict:
    reader = Reader(data)
    if reader.uint() != 0x03000002:
        raise ValueError('Unsupported irradiance index version')
    names = [reader.string() for _ in range(reader.count(4))]
    # Native loader 0x1800b65f4..0x1800b664a: each branch stores an
    # opaque ID followed by a counted list of local scenario indices.
    branches = []
    for _ in range(reader.count(8)):
        branch_id = reader.uint()
        indices = [reader.uint() for _ in range(reader.count(4))]
        if any(i >= len(names) for i in indices):
            raise ValueError('Invalid irradiance scenario reference')
        branches.append(dict(branch_id=branch_id, scenario_indices=indices,
                             scenario_names=[names[i] for i in indices],
                             local_state_mask=sum(1 << i for i in set(indices))))
    files = [reader.string() for _ in range(reader.count(4))]
    if not files or any(Path(name).name != name or '/' in name or '\\' in name for name in files):
        raise ValueError('Invalid irradiance file name')
    # Preserve unverified native configuration instead of assigning guessed units.
    configuration = reader.take(116).hex()
    mappings = [list(struct.unpack('<2I', reader.take(8))) for _ in range(reader.count(8))]
    tiles = []
    for i in range(reader.count(32)):
        coordinate, opaque, offset, size, header, payload, flags, parent = struct.unpack('<8I', reader.take(32))
        file_index = flags & 65535
        if file_index >= len(files) or size != header + payload or header < 8:
            raise ValueError('Invalid irradiance tile range or file reference')
        tiles.append(dict(index=i, coordinate=[coordinate >> shift & 255 for shift in (0, 8, 16, 24)],
                          opaque=opaque, file_offset=offset, bytes=size, header_bytes=header,
                          payload_bytes=payload, file_index=file_index, flags=flags >> 16, parent=parent))
    roots = [list(struct.unpack('<5I', reader.take(20))) for _ in range(reader.count(20))]
    if reader.offset != len(data):
        raise ValueError('Unconsumed irradiance index data')
    for tile in tiles:
        if tile['parent'] != 0xffffffff and tile['parent'] >= len(tiles):
            raise ValueError('Invalid irradiance parent reference')
    return dict(schema='NativeIrradianceIndex/2', source_sha256=hashlib.sha256(data).hexdigest(),
                scenario_names=names, branches=branches, files=files,
                raw_configuration=configuration, mappings=mappings, tiles=tiles, roots=roots)


def decode_amplitude(sample: bytes) -> list[float]:
    if len(sample) != 16:
        raise ValueError('Irradiance sample requires sixteen bytes')
    word = struct.unpack_from('<I', sample)[0]
    result = [struct.unpack('<e', struct.pack('<H', bits))[0] for bits in
              ((word & 2047) << 4, (word >> 7) & 32752, (word >> 17) & 32736)]
    if not all(math.isfinite(value) for value in result):
        raise ValueError('Nonfinite irradiance amplitude')
    return result


def decode_directional_coefficients(sample: bytes) -> list[list[float]]:
    """Return per-color [linear x,y,z,DC] for the direct V3 volume path.

    Pixel source: deferred segment1-1001480, lines 2295..2411. Directional
    RGB is remapped from UNORM by 4*u-2, then multiplied by amplitude.
    Alpha in each directional RGBA group is not the DC coefficient.
    """
    amplitudes = decode_amplitude(sample)
    return [[amplitude * (4 * value / 255 - 2) for value in sample[4 + 4*c:7 + 4*c]]
            + [amplitude] for c, amplitude in enumerate(amplitudes)]


def evaluate_directional_sample(sample: bytes, normal: tuple[float, float, float],
                                directional_strength: tuple[float, float, float] = (1, 1, 1)) -> list[float]:
    """Evaluate one sample with default SH=0 and optional visibility inactive.

    This is the isolated volume contribution, not complete scene GI.
    The caller supplies a unit world normal and per-color directional strength.
    Shader lines 2520..2531 clamp after summation; interpolated/blended samples
    must therefore combine coefficients before this final evaluation.
    """
    if (len(normal) != 3 or any(not math.isfinite(x) for x in normal)
            or abs(sum(x*x for x in normal) - 1) > 1e-4
            or len(directional_strength) != 3 or any(not math.isfinite(x) for x in directional_strength)):
        raise ValueError('Directional evaluation requires a unit normal and finite strength')
    return [max(coeff[3] + strength * sum(c*n for c, n in zip(coeff[:3], normal)), 0)
            for coeff, strength in zip(decode_directional_coefficients(sample), directional_strength)]


def world_address(position: tuple[float, float, float], lod: int) -> dict:
    """Address one V3 lattice sample, before interpolation/fallback.

    Static source: IrradianceVolumeUpdateV3CS Full/PartialUpdateClipmap0/1/2.
    The native world-domain guard is strict abs(position) < 2048.
    """
    if (len(position) != 3 or any(not math.isfinite(x) or abs(x) >= 2048 for x in position)
            or type(lod) is not int or not 0 <= lod <= 2):
        raise ValueError('Invalid V3 irradiance world position or LOD')
    q = [math.floor(2 * x + .5) + 4096 for x in position]
    outer = [(x >> (6 + lod)) << lod for x in q]
    brick = [(x >> (2 + 2 * lod)) & ((16 >> lod) - 1) for x in q]
    local = [(x >> (2 * lod)) & 3 for x in q]
    bit = sum(((value >> b) & 1) << (3 * b + axis)
              for axis, value in enumerate(local) for b in range(2))
    return dict(tile=outer + [lod], brick=brick, local=local,
                occupancy_bit=bit, quantized=q)


def query_world(directory: Path, position: tuple[float, float, float], lod: int,
                file_index: int) -> dict:
    """Read exact source bytes for an explicitly selected file and LOD.

    Missing samples return None; clipmap fallback, region overrides and
    trilinear interpolation belong to the consuming renderer.
    """
    address = world_address(position, lod)
    index = parse_index((directory / 'index.bytes').read_bytes())
    if type(file_index) is not int or not 0 <= file_index < len(index['files']):
        raise ValueError('Irradiance file selection is outside index')
    rows = [row for row in index['tiles']
            if row['file_index'] == file_index and row['coordinate'] == address['tile']]
    if len(rows) > 1:
        raise ValueError('Ambiguous irradiance world tile')
    result = dict(world=list(position), lod=lod, file_index=file_index, address=address,
                  source_file=index['files'][file_index], tile_index=None, sample=None)
    if rows:
        row = rows[0]
        with (directory / result['source_file']).open('rb') as source:
            source.seek(row['file_offset'])
            tile = parse_sparse_tile(source.read(row['bytes']), row['header_bytes'])
        result['tile_index'] = row['index']
        result['sample'] = tile.sample(tuple(address['brick']), address['occupancy_bit'])
        if result['sample']:
            result['sample']['file_byte_offset'] = row['file_offset'] + result['sample']['byte_offset']
    return result


@dataclass
class SparseTile:
    data: bytes
    m: int
    r: int
    header_bytes: int
    bricks: dict[tuple[int, int, int], tuple[int, int]]

    def sample(self, brick: tuple[int, int, int], bit: int) -> dict | None:
        if len(brick) != 3 or any(type(x) is not int or not 0 <= x <= 255 for x in brick):
            raise ValueError('Invalid source brick coordinate')
        if type(bit) is not int or not 0 <= bit < 64:
            raise ValueError('Invalid source occupancy bit')
        entry = self.bricks.get(tuple(brick))
        if entry is None or not (entry[1] >> bit) & 1:
            return None
        pointer, occupancy = entry
        rank = (occupancy & ((1 << bit) - 1)).bit_count()
        offset = pointer + rank * 16
        raw = self.data[offset:offset + 16]
        return dict(brick=list(brick), occupancy_bit=bit, sample_rank=rank, byte_offset=offset,
                    amplitude=decode_amplitude(raw), directional_rgba8=list(raw[4:]),
                    directional_atlas_unorm=[[x / 255 for x in raw[i:i + 4]] for i in (4, 8, 12)],
                    directional_coefficients=decode_directional_coefficients(raw),
                    raw_sha256=hashlib.sha256(raw).hexdigest())


def parse_sparse_tile(data: bytes, header_bytes: int) -> SparseTile:
    if len(data) < 8:
        raise ValueError('Truncated irradiance tile')
    m, r = struct.unpack_from('<2I', data)
    if not m or not r or m > 65535 or r > 65535:
        raise ValueError('Invalid irradiance hash dimensions')
    start = 8 + 4 * r ** 3
    if header_bytes != start + 16 * m ** 3 or header_bytes > len(data):
        raise ValueError('Irradiance header size differs')
    bricks = {}
    ranges = []
    for slot in range(m ** 3):
        key, pointer, occupancy = struct.unpack_from('<IIQ', data, start + 16 * slot)
        if key == 0xffffffff:
            continue
        xyz = tuple(key >> shift & 255 for shift in (0, 8, 16))
        if key >> 24 or xyz in bricks:
            raise ValueError('Invalid or duplicate irradiance hash key')
        displacement_index = xyz[0] % r + (xyz[1] % r) * r + (xyz[2] % r) * r * r
        displacement = struct.unpack_from('<I', data, 8 + 4 * displacement_index)[0]
        hashed = [(value + (displacement >> shift & 255)) % m for value, shift in zip(xyz, (0, 8, 16))]
        if hashed[0] + hashed[1] * m + hashed[2] * m * m != slot:
            raise ValueError('Irradiance spatial hash mismatch')
        end = pointer + occupancy.bit_count() * 16
        if not occupancy or pointer < header_bytes or end > len(data):
            raise ValueError('Invalid irradiance payload range')
        bricks[xyz] = (pointer, occupancy)
        ranges.append((pointer, end))
    cursor = header_bytes
    for begin, end in sorted(ranges):
        if begin != cursor:
            raise ValueError('Overlapping or incomplete irradiance payload')
        cursor = end
    if cursor != len(data):
        raise ValueError('Unconsumed irradiance payload')
    return SparseTile(data, m, r, header_bytes, bricks)


def inspect_directory(directory: Path, tile_index: int | None = None,
                      brick: tuple[int, int, int] | None = None, bit: int | None = None) -> dict:
    index = parse_index((directory / 'index.bytes').read_bytes())
    buffers = [(directory / name).read_bytes() for name in index['files']]
    counts = dict(tiles=0, bricks=0, samples=0)
    result = dict(index=index, counts=counts, source_files=[dict(name=name, bytes=len(data),
                  sha256=hashlib.sha256(data).hexdigest()) for name, data in zip(index['files'], buffers)])
    cursors = [0] * len(buffers)
    for row in index['tiles']:
        source = buffers[row['file_index']]
        offset, size = row['file_offset'], row['bytes']
        if offset != cursors[row['file_index']] or offset + size > len(source):
            raise ValueError('Invalid sparse file coverage')
        tile = parse_sparse_tile(source[offset:offset + size], row['header_bytes'])
        cursors[row['file_index']] = offset + size
        counts['tiles'] += 1
        counts['bricks'] += len(tile.bricks)
        counts['samples'] += sum(mask.bit_count() for _, mask in tile.bricks.values())
        if row['index'] == tile_index:
            if brick is None or bit is None:
                raise ValueError('Tile query requires source brick and occupancy bit')
            sample = tile.sample(brick, bit)
            if sample:
                sample['file_byte_offset'] = offset + sample['byte_offset']
            result['query'] = dict(tile_index=tile_index, source_file=index['files'][row['file_index']], sample=sample)
    if cursors != [len(data) for data in buffers]:
        raise ValueError('Incomplete sparse source file coverage')
    if tile_index is not None and 'query' not in result:
        raise ValueError('Tile query is outside index')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--tile', type=int)
    parser.add_argument('--brick', type=int, nargs=3)
    parser.add_argument('--bit', type=int)
    parser.add_argument('--world', type=float, nargs=3)
    parser.add_argument('--lod', type=int)
    parser.add_argument('--file-index', type=int)
    args = parser.parse_args()
    if args.world is not None:
        if args.lod is None or args.file_index is None or any(x is not None for x in (args.tile, args.brick, args.bit)):
            parser.error('--world requires --lod and --file-index, with no tile/brick/bit query')
        result = query_world(args.directory, tuple(args.world), args.lod, args.file_index)
    else:
        if args.lod is not None or args.file_index is not None:
            parser.error('--lod and --file-index require --world')
        result = inspect_directory(args.directory, args.tile, tuple(args.brick) if args.brick else None, args.bit)
    args.output.write_text(json.dumps(result, indent=2), encoding='utf-8')
    summary = result['counts'] if 'counts' in result else dict(
        tile_index=result['tile_index'], found=result['sample'] is not None)
    print(json.dumps(summary))
