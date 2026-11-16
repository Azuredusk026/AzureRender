"""Index existing Recovery and original Windows window captures."""

import argparse
from collections import Counter
import json
from pathlib import Path
import re

from PIL import Image

from tools.npr_room.reference_inventory import digest, text


def source_cases(serialized):
    block = re.search(r"\benum\s+WindowViewType\s*:\s*byte\s*\{([^}]*)\}", serialized)
    if not block:
        raise ValueError("Missing WindowViewType enumeration")
    result, names = {}, set()
    for entry in block[1].strip().removesuffix(",").split(","):
        match = re.fullmatch(r"\s*(\w+)\s*=\s*(\d+)\s*", entry)
        if not match:
            raise ValueError("Expected an explicit window identity")
        name, case = match[1], int(match[2])
        if case in result or name in names:
            raise ValueError("Duplicate window identity")
        result[case] = name
        names.add(name)
    return result


def capture(path, formats=("PNG",)):
    with Image.open(path) as picture:
        if picture.format not in formats:
            raise ValueError(f"Unexpected capture format {picture.format}: {path.name}")
        picture.load()
        return {"file": path.name, "size": list(picture.size), "mode": picture.mode,
                "format": picture.format, "sha256": digest(path)}


def isolated_captures(folder, cases):
    expected = {f"{case:02d}-{name}.png": case for case, name in cases.items()}
    existing = {path.name for path in folder.glob("*.png")}
    if expected.keys() - existing:
        raise ValueError(f"Missing reference captures: {sorted(expected.keys() - existing)}")
    return {case: {"case": cases[case], **capture(folder / name)} for name, case in sorted(expected.items())}


def build_report(recovery, enum_source):
    evidence = recovery / "Evidence"
    matrix_path = evidence / "window-matrix-result.json"
    matrix = json.loads(text(matrix_path))["data"]
    test, = [row for row in matrix["result"]["results"]
             if row["fullName"] == "WindowVisualAcceptance.EveryWindowViewActivatesAndRenders"]
    if matrix["status"] != "succeeded" or test["state"] != "Passed":
        raise ValueError("Recovery isolated window capture test is incomplete")
    cases = source_cases(text(enum_source))
    isolated = isolated_captures(evidence / "WindowMatrixIsolated", cases)
    original_folder = evidence / "OriginalGameCapture"
    manifest_path = original_folder / "capture-manifest.json"
    manifest = json.loads(text(manifest_path))
    original = {}
    for entry in manifest["screenshots"]:
        name = Path(entry["file"]).name
        data = capture(original_folder / name, ("PNG", "JPEG"))
        if name in original or data["size"] != entry["capturedSize"]:
            raise ValueError(f"Original capture identity or dimensions differ: {name}")
        original[name] = data
    isolated_dimensions = Counter("x".join(map(str, item["size"])) for item in isolated.values())
    return {
        "formatVersion": 1, "sourceEnumSHA256": digest(enum_source),
        "recoveryResultSHA256": digest(matrix_path), "originalManifestSHA256": digest(manifest_path),
        "isolated": {"mode": matrix["mode"], "passed": True, "dimensions": dict(isolated_dimensions),
                     "distinctImages": len({row["sha256"] for row in isolated.values()}), "cases": isolated},
        "original": {"configuredResolution": manifest["configuredResolution"], "quality": manifest["quality"],
                     "renderScale": manifest["renderScale"], "captures": original},
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--recovery", type=Path, required=True)
    parser.add_argument("--enum-source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report = build_report(args.recovery, args.enum_source)
    with args.output.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(report, stream, sort_keys=True, separators=(",", ":"))
        stream.write("\n")
    print(json.dumps({"isolated": len(report["isolated"]["cases"]),
                      "distinctImages": report["isolated"]["distinctImages"],
                      "original": len(report["original"]["captures"])}))


if __name__ == "__main__":
    main()
