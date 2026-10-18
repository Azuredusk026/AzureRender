"""Convert decoded DDS cubemap levels to authored linear HDR .azenv."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import numpy as np


def convert(source, output, face_size, mips):
    if face_size < 1 or mips < 1 or mips > face_size.bit_length():
        raise ValueError('Invalid cubemap dimensions or mip count')
    width, height = face_size * 4, face_size * 2
    payload = bytearray(b'AZENV001' + struct.pack('<III', width, height, mips))
    inputs = []
    maximum = 0.
    for mip in range(mips):
        size = max(1, face_size >> mip)
        u = (np.arange(width) + .5) / width
        v = (np.arange(height) + .5) / height
        phi = (u - .5) * (2 * np.pi)
        theta = v * np.pi
        x = np.sin(theta[:, None]) * np.cos(phi[None, :])
        y = np.broadcast_to(np.cos(theta[:, None]), x.shape)
        z = np.sin(theta[:, None]) * np.sin(phi[None, :])
        direction = np.stack((x, y, z), -1)
        major = np.argmax(np.abs(direction), -1)
        face = major * 2 + (np.take_along_axis(direction, major[..., None], -1)[..., 0] < 0)
        magnitude = np.max(np.abs(direction), -1)
        image = np.zeros((height, width, 4), dtype='<f4')
        # DDS: +X,-X,+Y,-Y,+Z,-Z. This is EnvironmentAsset.cpp's basis.
        for index, (horizontal, vertical) in enumerate(((-z,-y),(z,-y),(x,z),(x,-z),(x,-y),(-x,-y))):
            path = source / f'face{index}_mip{mip}.rgba32f'
            raw = path.read_bytes()
            if len(raw) != size * size * 16:
                raise ValueError(f'Incorrect decoded cubemap size: {path}')
            pixels = np.frombuffer(raw, dtype='<f4').reshape(size, size, 4)
            if not np.all(np.isfinite(pixels)) or np.any(np.abs(pixels)>65504):
                raise ValueError(f'Cubemap exceeds finite HDR range: {path}')
            inputs.append({'file': str(path.resolve()), 'sha256': hashlib.sha256(raw).hexdigest()})
            maximum = max(maximum, float(np.max(pixels[..., :3])))
            mask = face == index
            sx = np.clip((horizontal[mask] / magnitude[mask] * .5 + .5) * size - .5, 0, size-1)
            sy = np.clip((vertical[mask] / magnitude[mask] * .5 + .5) * size - .5, 0, size-1)
            ix, iy = sx.astype(int), sy.astype(int)
            jx, jy = np.minimum(ix+1, size-1), np.minimum(iy+1, size-1)
            fx, fy = (sx-ix)[:, None], (sy-iy)[:, None]
            image[mask] = ((pixels[iy, ix]*(1-fx)+pixels[iy, jx]*fx)*(1-fy)
                           +(pixels[jy, ix]*(1-fx)+pixels[jy, jx]*fx)*fy)
        payload.extend(image.astype('<f2').tobytes())
        width, height = max(1, width//2), max(1, height//2)
    output.write_bytes(payload)
    report = {'schema': 'CubemapEnvironmentConversion/1', 'mips': mips,
        'source_face_size': face_size, 'output': str(output.resolve()),
        'sha256': hashlib.sha256(payload).hexdigest(), 'maximum_source_rgb': maximum,
        'orientation': 'DDS basis matching EnvironmentAsset.cpp; game parity requires reference',
        'resampling': 'bilinear per authored level; RGBA16F linear; no tone mapping', 'inputs': inputs}
    output.with_suffix(output.suffix+'.json').write_text(json.dumps(report, indent=2))
    return report


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--face-size',type=int,required=True)
    parser.add_argument('--mips',type=int,required=True)
    args=parser.parse_args()
    report=convert(args.source,args.output,args.face_size,args.mips)
    print(json.dumps({key:value for key,value in report.items() if key!='inputs'}))
