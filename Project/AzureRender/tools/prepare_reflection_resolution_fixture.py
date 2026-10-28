"""Restore the bounded native depth fixture for the GPU reduction test."""
import argparse
import hashlib
import json
from pathlib import Path
import zlib


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    fixture = root / 'docs/acceptance/nr/evidence/2026-10-28/reflection-resolution-fixture'
    metadata = json.loads((fixture / 'fixture.json').read_text(encoding='utf-8'))
    compressed = (fixture / 'full-depth-r32.zlib').read_bytes()
    assert len(compressed) == metadata['compressedBytes'] <= 16 * 1024 * 1024
    assert hashlib.sha256(compressed).hexdigest() == metadata['compressedSha256']
    expected = metadata['uncompressedBytes']
    assert expected == metadata['width'] * metadata['height'] * 4 <= 16 * 1024 * 1024
    decoder = zlib.decompressobj()
    data = decoder.decompress(compressed, expected + 1)
    assert decoder.eof and not decoder.unused_data and not decoder.unconsumed_tail
    assert len(data) == expected and hashlib.sha256(data).hexdigest() == metadata['sha256']
    output = args.output.resolve()
    if not output.is_relative_to(root / 'build/runs'):
        raise ValueError('Fixture output must stay in the workspace build/runs directory')
    with output.open('xb') as stream:
        stream.write(data)
    print(json.dumps(dict(path=str(output), bytes=len(data), sha256=metadata['sha256'])))


if __name__ == '__main__':
    main()
