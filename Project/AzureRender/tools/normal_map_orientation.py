"""Convert encoded tangent normal Y orientation without changing other channels."""
import argparse
import hashlib
import json
from pathlib import Path
from PIL import Image


def convert_normal_y(image, source_y, target_y):
    if source_y not in ('positive', 'negative') or target_y not in ('positive', 'negative'):
        raise ValueError('Normal Y orientations must be positive or negative')
    if not isinstance(image, Image.Image) or image.mode not in ('RGB', 'RGBA'):
        raise ValueError('Normal orientation requires encoded RGB or RGBA bytes')
    if source_y == target_y:
        return image.copy()
    channels = list(image.split())
    channels[1] = channels[1].point(lambda value: 255-value)
    return Image.merge(image.mode, channels)


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--source-y', choices=['positive', 'negative'], required=True)
    parser.add_argument('--target-y', choices=['positive', 'negative'], required=True)
    args = parser.parse_args()
    provenance = args.output.with_suffix('.png.orientation.json')
    if (args.output.exists() or provenance.exists() or args.input.resolve() == args.output.resolve()
            or args.output.suffix.lower() != '.png'):
        raise ValueError('Output must be a new PNG asset')
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    before = sha(args.input)
    with Image.open(args.input) as source:
        output = convert_normal_y(source, args.source_y, args.target_y)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('xb') as target:
        output.save(target, format='PNG')
    if sha(args.input) != before:
        raise ValueError('Normal source changed during conversion')
    receipt = dict(format='NormalYOrientation/1', source=str(args.input.resolve()),
                   sourceSha256=before, output=str(args.output.resolve()), outputSha256=sha(args.output),
                   sourceY=args.source_y, targetY=args.target_y, mode=output.mode, size=list(output.size),
                   toolSha256=sha(Path(__file__)))
    with provenance.open('x', encoding='utf8') as target:
        target.write(json.dumps(receipt, indent=2)+'\n')
    print(json.dumps(receipt))


if __name__ == '__main__':
    main()
