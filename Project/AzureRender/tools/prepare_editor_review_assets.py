"""Prepare public, self-contained inputs for the editor review workflow."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import shutil
import struct
import wave
from PIL import Image


def prepare(output):
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    source = Path(__file__).resolve().parents[1]
    model = source / 'assets_public/scene_inspector/assets/cube.gltf'
    shutil.copyfile(model, output / 'review-cube.gltf')
    image = Image.new('RGBA', (128, 128))
    image.putdata([(50, 160, 220, 255) if (x // 16 + y // 16) % 2 else (220, 100, 50, 192)
                   for y in range(128) for x in range(128)])
    image.save(output / 'review-checker.png')
    with wave.open(str(output / 'review-tone.wav'), 'wb') as audio:
        audio.setparams((1, 2, 48000, 0, 'NONE', 'not compressed'))
        audio.writeframes(b''.join(struct.pack('<h', round(3200 * math.sin(2 * math.pi * 440 * i / 48000)))
                                  for i in range(48000)))
    records = []
    for name, origin in [('review-cube.gltf', model.relative_to(source).as_posix()),
                         ('review-checker.png', 'procedural checker, RGBA 128x128'),
                         ('review-tone.wav', 'procedural sine, mono PCM16 48000Hz 440Hz 1s')]:
        records.append(dict(file=name, source=origin, license='CC0-1.0',
                            sha256=hashlib.sha256((output / name).read_bytes()).hexdigest()))
    shutil.copyfile(model.parent / 'ASSET-LICENSE.txt', output / 'LICENSE')
    manifest = dict(schemaVersion=1, assets=records, externalDependencies=False)
    (output / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    return manifest


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(prepare(args.output)))
