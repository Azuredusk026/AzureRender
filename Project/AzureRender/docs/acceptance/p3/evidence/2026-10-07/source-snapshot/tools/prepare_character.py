"""Create a traceable GLB candidate with explicit brow material settings."""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def prepare(source: Path, output: Path, *, brow_double_sided: bool = False):
    source, output = Path(source), Path(output)
    if source.resolve() == output.resolve():
        raise ValueError("Candidate must use a separate path")
    if output.exists():
        raise FileExistsError(output)
    data = source.read_bytes()
    if len(data) < 20 or struct.unpack_from("<III", data) != (0x46546c67, 2, len(data)):
        raise ValueError("Invalid GLB header")
    length, kind = struct.unpack_from("<II", data, 12)
    if kind != 0x4e4f534a or length % 4 or 20 + length > len(data):
        raise ValueError("Invalid leading JSON chunk")
    document = json.loads(data[20:20 + length])
    cursor = 20 + length
    while cursor < len(data):
        if cursor + 8 > len(data):
            raise ValueError("Truncated GLB chunk")
        size = struct.unpack_from("<I", data, cursor)[0]
        if size % 4 or cursor + 8 + size > len(data):
            raise ValueError("Invalid GLB chunk size")
        cursor += 8 + size
    corrected = []
    if brow_double_sided:
        for index, material in enumerate(document.get("materials", [])):
            profile = material.get("extras", {}).get("azureRenderMaterial", {})
            if "brow-overlay" in profile.get("features", []):
                material["doubleSided"] = True
                corrected.append(index)
        if not corrected:
            raise ValueError("No material declares brow-overlay")
    document.setdefault("asset", {}).setdefault("extras", {})["azureCharacterPreparation"] = {
        "version": 1, "sourceSha256": hashlib.sha256(data).hexdigest(),
        "browDoubleSidedMaterials": corrected}
    payload = json.dumps(document, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    payload += b" " * (-len(payload) % 4)
    tail = data[20 + length:]
    result = struct.pack("<III", 0x46546c67, 2, 20 + len(payload) + len(tail))
    result += struct.pack("<II", len(payload), 0x4e4f534a) + payload + tail
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("xb") as stream:
        stream.write(result)
    return {"sourceSha256": hashlib.sha256(data).hexdigest(),
            "candidateSha256": hashlib.sha256(result).hexdigest(), "materials": corrected}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--brow-double-sided", action="store_true")
    args = parser.parse_args()
    print(json.dumps(prepare(args.input, args.output, brow_double_sided=args.brow_double_sided), indent=2))
