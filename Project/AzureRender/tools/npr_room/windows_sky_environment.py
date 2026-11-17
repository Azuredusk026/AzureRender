"""Transfer an original Direct3D sky capture into a linear Vulkan environment map."""

import argparse
import gzip
from hashlib import sha256
import json
from pathlib import Path

import numpy as np

from tools.npr_room.reference_inventory import digest
from tools.npr_room.windows_sky_probe import FACES


def sample_sky(cube, azure_directions):
    if cube.ndim != 4 or cube.shape[0] != 6 or cube.shape[1] != cube.shape[2] or cube.shape[3] != 3:
        raise ValueError("Expected six square RGB faces")
    directions = np.asarray(azure_directions) * [1, 1, -1]
    forward, up = np.asarray(FACES).transpose(1, 0, 2)
    right = np.cross(up, forward)
    projection = directions @ forward.T
    faces = projection.argmax(axis=-1)
    depth = np.take_along_axis(projection, faces[..., None], axis=-1)[..., 0]
    if not np.all(np.isfinite(depth) & (depth > 0)):
        raise ValueError("Cannot project non-finite or zero sky directions")
    extent = cube.shape[1]
    horizontal = (directions * right[faces]).sum(axis=-1) / depth
    vertical = -(directions * up[faces]).sum(axis=-1) / depth
    coords = np.clip((np.stack((horizontal, vertical), axis=-1) + 1) * extent / 2 - .5, 0, extent - 1)
    low = coords.astype(np.intp)
    high = np.minimum(low + 1, extent - 1)
    x0, y0 = low[..., 0], low[..., 1]
    x1, y1 = high[..., 0], high[..., 1]
    x, y = (coords - low)[..., 0, None], (coords - low)[..., 1, None]
    first = cube[faces, y0, x0] * (1 - x) + cube[faces, y0, x1] * x
    second = cube[faces, y1, x0] * (1 - x) + cube[faces, y1, x1] * x
    return first * (1 - y) + second * y


def equirectangular(cube):
    size = cube.shape[1]
    width, height = size * 4, size * 2
    angle = ((np.arange(width) + .5) / width - .5) * 2 * np.pi
    elevation = (np.arange(height) + .5) / height * np.pi
    ring = np.sin(elevation)[:, None]
    direction = np.stack((ring * np.cos(angle), np.broadcast_to(np.cos(elevation)[:, None], (height, width)),
                          ring * np.sin(angle)), axis=-1)
    return sample_sky(cube, direction)


def rgbe(linear):
    if not np.all(np.isfinite(linear) & (linear >= 0)):
        raise ValueError("Radiance values must be finite and nonnegative")
    highest = linear.max(axis=-1)
    _, power = np.frexp(highest)
    nonzero = highest >= 1e-32
    if np.any(power >= 128):
        raise ValueError("Radiance values exceed RGBE storage")
    scale = np.exp2(8 - power.astype(np.float64))
    output = np.empty((*highest.shape, 4), dtype=np.uint8)
    output[..., :3] = (linear * np.where(nonzero, scale, 0)[..., None]).astype(np.uint8)
    output[..., 3] = np.where(nonzero, power + 128, 0)
    return output


def write_hdr(stream, encoded):
    height, width, _ = encoded.shape
    if not 8 <= width <= 32767:
        raise ValueError("Radiance RLE requires a width from 8 through 32767")
    stream.write(f"#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y {height} +X {width}\n".encode("ascii"))
    for scanline in encoded:
        stream.write(bytes((2, 2, width >> 8, width & 255)))
        for channel in scanline.T:
            for start in range(0, width, 128):
                literal = channel[start:start + 128]
                stream.write(bytes((len(literal),)))
                stream.write(literal.tobytes())


def convert(capture, reference, output):
    receipt = json.loads(reference.read_bytes())
    payload = gzip.decompress(capture.read_bytes())
    if sha256(payload).hexdigest() != receipt["rawSHA256"] or receipt["format"] != "RGBA32F":
        raise ValueError("The original sky cube does not match its native Direct3D receipt")
    if receipt["faceOrder"] != ["+X", "-X", "+Y", "-Y", "+Z", "-Z"]:
        raise ValueError("Expected the source capture's D3D face order")
    width, height = receipt["resolution"]
    source = np.frombuffer(payload, dtype="<f4").reshape(6, height, width, 4)
    if not np.all(source[..., 3] == 1):
        raise ValueError("The sky cube requires complete radiance coverage")
    linear = equirectangular(source[..., :3])
    encoded = rgbe(linear)
    with output.open("xb") as stream:
        write_hdr(stream, encoded)
    scale = np.exp2(encoded[..., 3].astype(np.float64) - 136)
    error = np.abs(linear - encoded[..., :3] * scale[..., None])
    return {"sourceSHA256": receipt["rawSHA256"], "environmentSHA256": digest(output),
            "width": linear.shape[1], "height": linear.shape[0], "encodingMaxAbsoluteError": float(error.max())}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("capture", "reference", "output"):
        parser.add_argument("--" + option, type=Path, required=True)
    args = parser.parse_args()
    proof = convert(args.capture, args.reference, args.output)
    print(json.dumps(proof))
