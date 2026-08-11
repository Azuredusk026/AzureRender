#!/usr/bin/env python3
"""Validate AzureRender glTF/GLB material profiles without third-party packages."""

from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path


def load_gltf(path: Path) -> dict:
    if path.suffix.lower() == ".gltf":
        return json.loads(path.read_text(encoding="utf-8-sig"))
    data = path.read_bytes()
    if len(data) < 20 or data[:4] != b"glTF":
        raise ValueError(f"Not a glTF 2.0 GLB: {path}")
    version = struct.unpack_from("<I", data, 4)[0]
    chunk_length, chunk_type = struct.unpack_from("<II", data, 12)
    if version != 2 or chunk_type != 0x4E4F534A:
        raise ValueError(f"GLB lacks a leading JSON chunk: {path}")
    return json.loads(data[20 : 20 + chunk_length].decode("utf-8").rstrip("\x00 "))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("asset", type=Path)
    parser.add_argument(
        "--schema",
        type=Path,
        default=Path(__file__).resolve().parents[1]
        / "schemas"
        / "azure_render_material.schema.json",
    )
    args = parser.parse_args()

    schema = json.loads(args.schema.read_text(encoding="utf-8-sig"))
    document = load_gltf(args.asset)
    allowed_classes = set(schema["properties"]["class"]["enum"])
    allowed_features = set(
        schema["properties"]["features"]["items"]["enum"]
    )
    required = set(schema["required"])
    allowed = set(schema["properties"])
    vector_rule = schema["$defs"]["parameterVector"]
    materials = document.get("materials", [])
    errors: list[str] = []

    for index, material in enumerate(materials):
        name = material.get("name") or f"material-{index}"
        profile = material.get("extras", {}).get("azureRenderMaterial")
        if not isinstance(profile, dict):
            errors.append(f"{name}: missing azureRenderMaterial object")
            continue
        missing = required - set(profile)
        unexpected = set(profile) - allowed
        if missing:
            errors.append(f"{name}: missing {sorted(missing)}")
        if unexpected:
            errors.append(f"{name}: unexpected {sorted(unexpected)}")
        if profile.get("schemaVersion") != 1:
            errors.append(f"{name}: schemaVersion must be 1")
        if profile.get("class") not in allowed_classes:
            errors.append(f"{name}: unknown class {profile.get('class')!r}")
        features = profile.get("features")
        if not isinstance(features, list) or len(features) != len(set(features)):
            errors.append(f"{name}: features must be a unique array")
        elif set(features) - allowed_features:
            errors.append(
                f"{name}: unknown features {sorted(set(features) - allowed_features)}"
            )
        for key in ("styleParameters", "featureParameters"):
            vector = profile.get(key)
            valid = isinstance(vector, list) and len(vector) == 4
            if valid:
                valid = all(
                    isinstance(value, (int, float))
                    and vector_rule["items"]["minimum"] <= value
                    <= vector_rule["items"]["maximum"]
                    for value in vector
                )
            if not valid:
                errors.append(f"{name}: invalid {key}")
        print(
            f"[{index:02d}] {name} -> {profile.get('class', 'INVALID')} "
            f"features={profile.get('features', [])}"
        )

    if errors:
        for error in errors:
            print(f"ERROR: {error}")
        raise SystemExit(1)
    print(f"Validated {len(materials)} AzureRender Material Profile v1 entries")


if __name__ == "__main__":
    main()
