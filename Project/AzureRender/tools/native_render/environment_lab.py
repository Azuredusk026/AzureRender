"""Verify authored HDR levels survive the real Player upload and sampling path."""
import argparse
import json
from pathlib import Path
import struct
import subprocess

from light_lab import generate


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--player', type=Path, required=True)
    args = parser.parse_args()
    root = args.output.resolve()
    generate(root, 'dark')
    level = root / 'level.azurelevel'
    document = json.loads(level.read_text(encoding='utf8'))
    document['renderSettings']['skyIntensity'] = 1
    level.write_text(json.dumps(document), encoding='utf8')
    width, height = 256, 128
    payload = b'AZENV001' + struct.pack('<III', width, height, 8)
    for mip in range(8):
        # A runtime re-filter would produce red. The authored coarse levels are blue.
        color = (2, 0, 0, 1) if mip == 0 else (0, 0, 2, 1)
        payload += struct.pack('<4e', *color) * (width * height)
        width, height = max(1, width // 2), max(1, height // 2)
    (root / 'test.azenv').write_bytes(payload)
    with (root / 'player.log').open('w', encoding='utf8') as log:
        subprocess.run([str(args.player.resolve()), '--project', str(root / 'lab.azureproject'),
            '--environment', str(root / 'test.azenv'), '--width', '256', '--height', '256',
            '--capture-dir', str(root / 'frames'), '--capture-frames', '1', '--fixed-frame-step'],
            stdout=log, stderr=subprocess.STDOUT, check=True, timeout=120)
    from PIL import Image
    pixel = Image.open(root / 'frames/frame_000000.png').convert('RGB').getpixel((128, 128))
    report = {'center': pixel, 'expected': 'blue authored coarse mip',
              'passed': pixel[0] == 0 and pixel[1] == 0 and pixel[2] > 240}
    (root / 'metrics.json').write_text(json.dumps(report, indent=2), encoding='utf8')
    if not report['passed']:
        raise AssertionError(report)
    print(json.dumps(report))


if __name__ == '__main__':
    main()
