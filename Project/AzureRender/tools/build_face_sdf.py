#!/usr/bin/env python3
"""Build and optionally inject AzureRender Face SDF v1 into a GLB."""

from __future__ import annotations

import argparse
import json
import math
import struct
import zlib
from pathlib import Path


def chunk(kind: bytes, payload: bytes) -> bytes:
    crc = zlib.crc32(kind + payload) & 0xFFFFFFFF
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", crc)


def encode_png(width: int, height: int, pixels: bytes) -> bytes:
    rows = bytearray()
    stride = width * 4
    for row in range(height):
        rows.append(0)
        rows.extend(pixels[row * stride : (row + 1) * stride])
    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(
        b"IDAT", zlib.compress(bytes(rows), 9)
    ) + chunk(b"IEND", b"")


def build_sdf(width: int, height: int) -> bytes:
    pixels = bytearray(width * height * 4)
    for y in range(height):
        v = (y + 0.5) / height
        # Face UVs are vertically flipped relative to the inspected PNG.
        image_y = 1.0 - v
        for x in range(width):
            u = (x + 0.5) / width
            # Conservative authored face island. A is participation, R is
            # the left-to-right signed face coordinate used by the shader.
            ellipse = math.sqrt(
                ((u - 0.50) / 0.39) ** 2 + ((image_y - 0.52) / 0.34) ** 2
            )
            participation = 1.0 - smoothstep(0.92, 1.02, ellipse)
            # Remove the lower mouth/neck islands from the large-scale face mask.
            participation *= 1.0 - smoothstep(0.73, 0.84, image_y)
            coordinate = max(0.0, min(1.0, u))
            pixel = (y * width + x) * 4
            pixels[pixel + 0] = round(coordinate * 255.0)
            pixels[pixel + 1] = 128
            pixels[pixel + 2] = 128
            pixels[pixel + 3] = round(participation * 255.0)
    return encode_png(width, height, bytes(pixels))


def smoothstep(edge0: float, edge1: float, value: float) -> float:
    t = max(0.0, min(1.0, (value - edge0) / (edge1 - edge0)))
    return t * t * (3.0 - 2.0 * t)


def parse_glb(path: Path) -> tuple[dict, bytes]:
    data = path.read_bytes()
    if data[:4] != b"glTF" or struct.unpack_from("<I", data, 4)[0] != 2:
        raise ValueError(f"Not a glTF 2.0 GLB: {path}")
    json_length, json_type = struct.unpack_from("<II", data, 12)
    if json_type != 0x4E4F534A:
        raise ValueError("GLB JSON chunk missing")
    document = json.loads(data[20 : 20 + json_length].decode("utf-8").rstrip("\x00 "))
    binary_offset = 20 + json_length
    binary_length, binary_type = struct.unpack_from("<II", data, binary_offset)
    if binary_type != 0x004E4942:
        raise ValueError("GLB BIN chunk missing")
    return document, data[binary_offset + 8 : binary_offset + 8 + binary_length]


def write_glb(path: Path, document: dict, binary: bytes) -> None:
    json_bytes = json.dumps(document, separators=(",", ":"), ensure_ascii=True).encode()
    json_bytes += b" " * ((4 - len(json_bytes) % 4) % 4)
    binary += b"\x00" * ((4 - len(binary) % 4) % 4)
    total_length = 12 + 8 + len(json_bytes) + 8 + len(binary)
    output = bytearray(struct.pack("<III", 0x46546C67, 2, total_length))
    output.extend(struct.pack("<II", len(json_bytes), 0x4E4F534A))
    output.extend(json_bytes)
    output.extend(struct.pack("<II", len(binary), 0x004E4942))
    output.extend(binary)
    path.write_bytes(output)


def inject(document: dict, binary: bytes, image: bytes) -> tuple[dict, bytes]:
    document.setdefault("bufferViews", [])
    document.setdefault("images", [])
    document.setdefault("textures", [])
    binary += b"\x00" * ((4 - len(binary) % 4) % 4)
    view_index = len(document["bufferViews"])
    document["bufferViews"].append(
        {"buffer": 0, "byteOffset": len(binary), "byteLength": len(image)}
    )
    binary += image
    image_index = len(document["images"])
    document["images"].append(
        {"name": "azure_face_sdf_v1", "mimeType": "image/png", "bufferView": view_index}
    )
    texture_index = len(document["textures"])
    document["textures"].append({"source": image_index})
    matched = 0
    for material in document.get("materials", []):
        if "face" not in (material.get("name") or "").lower():
            continue
        extras = material.setdefault("extras", {})
        profile = extras.setdefault("azureRenderMaterial", {})
        profile["faceSdf"] = {
            "schemaVersion": 1,
            "texture": texture_index,
            "texCoord": 0,
            "channel": "r",
            "maskChannel": "a",
            "shadowOnLowValues": True,
            "horizontalAxis": "left-to-right",
            "headNode": "Bip001_Head",
        }
        matched += 1
    if matched != 1:
        raise ValueError(f"Expected exactly one face material, found {matched}")
    document.setdefault("asset", {}).setdefault("extras", {})[
        "azureRenderFaceSdf"
    ] = {"format": "AzureRender Face SDF v1", "generator": "build_face_sdf.py"}
    return document, binary


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=root / "assets_public" / "face_sdf_v1.png")
    parser.add_argument("--input-glb", type=Path)
    parser.add_argument("--output-glb", type=Path)
    parser.add_argument("--width", type=int, default=1024)
    parser.add_argument("--height", type=int, default=1024)
    args = parser.parse_args()
    if args.width < 64 or args.height < 64:
        raise SystemExit("Face SDF dimensions must be at least 64x64")
    image = build_sdf(args.width, args.height)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(image)
    print(f"Wrote Face SDF: {args.output} ({args.width}x{args.height})")
    if args.input_glb or args.output_glb:
        if not args.input_glb or not args.output_glb:
            raise SystemExit("--input-glb and --output-glb must be used together")
        document, binary = parse_glb(args.input_glb)
        document, binary = inject(document, binary, image)
        args.output_glb.parent.mkdir(parents=True, exist_ok=True)
        write_glb(args.output_glb, document, binary)
        print(f"Wrote Face SDF GLB: {args.output_glb}")


if __name__ == "__main__":
    main()
