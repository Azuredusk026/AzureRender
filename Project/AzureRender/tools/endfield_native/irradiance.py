"""Read the observed native v3 irradiance index and sparse source payloads.

Evidence: native IrradianceVolumeAtlasBakeV3CS DXBC region upload and
IrradianceVolumeIndirectUpdateV2CS spatial-hash lookup. V3 file-range,
occupancy and hash identities were checked on both source scene branches.
Amplitude is the packed unsigned RGB term, not evaluated irradiance.
World-space lattice selection and scenario activation remain explicit inputs.
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
    branches = [list(struct.unpack('<2I', reader.take(8))) for _ in range(reader.count(8))]
    reserved = reader.uint()
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
    return dict(schema='NativeIrradianceIndex/1', source_sha256=hashlib.sha256(data).hexdigest(),
                scenario_names=names, branches=branches, reserved=reserved, files=files,
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
    args = parser.parse_args()
    result = inspect_directory(args.directory, args.tile, tuple(args.brick) if args.brick else None, args.bit)
    args.output.write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps(result['counts']))
