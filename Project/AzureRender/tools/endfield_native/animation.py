"""Numerical validation for generic QVVF clips with constant identity roots.

This deliberately narrow candidate path does not solve humanoid muscles,
moving root motion or Animator parameters. It preserves these source channels
and rejects their nonconstant/nonidentity cases instead of dropping them.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import zlib


def require(condition, message):
    if not condition:
        raise ValueError(message)


def verify_generic_candidate(audit, source, transforms, roots, scalars):
    count = audit["output_track_count"]
    bindings = audit["bindings"]
    require(count > 0 and len(bindings) == count * 2 + 17, "Unsupported generic binding layout")
    require(audit["root_track_count"] == 21 and audit["float_curve_count"] == 17, "Unsupported root/scalar layout")
    require(not audit["default_indices"], "Default bindings require explicit reconstruction")
    paths = {}
    for row in source["transforms"]:
        parts = row["path"].split("/")
        for index in range(len(parts)):
            paths.setdefault(zlib.crc32("/".join(parts[index:]).encode()), set()).add(row["path"])
    masks = audit["transform_sub_track_masks"]
    stride = (count + 7) // 8
    require(len(masks) == ((stride * 3 + 7) // 8) * 8, "Invalid mask allocation")
    expected_masks = []
    for component in range(3):
        for offset in range(stride):
            expected_masks.append(sum(1 << (7-bit) for bit in range(8) if offset*8+bit < count) if component < 2 else 0)
    expected_masks += [0] * (len(masks) - len(expected_masks))
    require(masks == expected_masks, "Unsupported position/rotation/scale masks")
    tracks = []
    for index in range(count):
        p, q = bindings[index], bindings[index+count]
        require(p["type"] == q["type"] == "Transform" and p["attribute"] == 1 and q["attribute"] == 2 and p["path"] == q["path"], "Misaligned transform bindings")
        candidates = paths.get(p["path"], set())
        require(len(candidates) == 1, "Missing or ambiguous source path")
        tracks.append({"track": index, "path_hash": p["path"], "source_path": next(iter(candidates))})
    require(len({t["source_path"] for t in tracks}) == count, "Duplicate source tracks")
    for index, binding in enumerate(bindings[count*2:]):
        require(binding["type"] == "Animator", "Unsupported scalar type")
        if index < 14:
            require(binding["custom_type"] == 8 and binding["attribute"] == index, "Unsupported scalar root binding")
        else:
            require(binding["custom_type"] == 0, "Humanoid muscle requires explicit solver")
    require(transforms and roots and scalars, "Empty sample buffers")
    require(len(roots) == len(scalars) and len(transforms) in (len(roots), len(roots)+1), "Incompatible frame domains")
    for frames in (transforms, roots, scalars):
        require(all(math.isfinite(f["time"]) for f in frames), "Nonfinite timestamp")
        require(all(a["time"] < b["time"] for a,b in zip(frames,frames[1:])), "Nonmonotonic timestamp")
    identity = [0, 0, 0, 0, 0, 0, 1]
    for i, (root, scalar) in enumerate(zip(roots, scalars)):
        require(abs(root["time"]-scalar["time"]) < 1e-6 and abs(root["time"]-transforms[i]["time"]) < 1e-6, "Misaligned sample times")
        require(len(root["values"]) == 21 and all(math.isfinite(v) and abs(v-identity[j%7]) < 1e-6 for j,v in enumerate(root["values"])), "Nonidentity root requires explicit solver")
        values = scalar["values"]
        require(len(values) == 17 and all(math.isfinite(v) for v in values), "Invalid scalar dimensions")
        require(values == scalars[0]["values"], "Animated scalar requires explicit driver")
        require(all(abs(values[j]-identity[j%7]) < 1e-6 for j in range(14)), "Nonidentity scalar root")
    max_rotation_error = 0.0
    for frame in transforms:
        require(len(frame["tracks"]) == count, "Invalid track dimensions")
        for i,t in enumerate(frame["tracks"]):
            require(t["track"] == i and len(t["translation"]) == 3 and len(t["rotation"]) == 4 and len(t["scale"]) == 3, "Invalid transform record")
            require(all(math.isfinite(v) for k in ("translation", "rotation", "scale") for v in t[k]), "Nonfinite transform")
            error = abs(sum(v*v for v in t["rotation"])-1)
            require(error < 0.0021, "Nonunit rotation")
            max_rotation_error = max(max_rotation_error,error)
            require(all(abs(v-1) < 1e-6 for v in t["scale"]), "Unbound scale animation")
    require(abs(transforms[0]["time"]-audit["start"]) < 1e-6 and abs(transforms[-1]["time"]-audit["stop"]) < 1e-5, "Incomplete transform time domain")
    require(all(isinstance(i,int) and 0 <= i < len(bindings) for i in audit["constant_indices"]), "Invalid constant binding")
    require(len(set(audit["constant_indices"])) == len(audit["constant_indices"]), "Duplicate constant binding")
    offset = 0
    constant_error = 0.0
    for binding in audit["constant_indices"]:
        width = 3 if binding < count else 4 if binding < count*2 else 1
        expected = audit["constant_values"][offset:offset+width]
        require(len(expected) == width, "Truncated constants")
        for index,frame in enumerate(transforms):
            values = (frame["tracks"][binding]["translation"] if binding < count else
                      frame["tracks"][binding-count]["rotation"] if binding < count*2 else
                      [scalars[min(index,len(scalars)-1)]["values"][binding-count*2]])
            error = max(abs(a-b) for a,b in zip(values,expected))
            require(error <= 1e-4, "Decoded constant disagrees with source")
            constant_error = max(constant_error,error)
        offset += width
    require(offset == len(audit["constant_values"]), "Unconsumed constant values")
    return {"status": "generic_qvvf_numerical_candidate", "tracks": tracks, "frames": len(transforms),
            "start": transforms[0]["time"], "stop": transforms[-1]["time"],
            "root_policy": "three_constant_identity_groups_preserved", "scalar_constants": scalars[0]["values"],
            "scalar_policy": "preserved_requires_controller_semantics_before_full_fidelity_claim",
            "maximum_rotation_norm_squared_error": max_rotation_error,
            "verified_constant_bindings": len(audit["constant_indices"]), "maximum_constant_error": constant_error,
            "limitations": ["Numerical candidate; rendering and game-runtime equivalence require separate validation.",
                            "Root group semantics are not inferred beyond verified constant identity values."]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native", type=Path, required=True)
    parser.add_argument("--source-audit", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    files = [args.native/"clip_audit.json", args.source_audit, *[args.native/(n+"_samples.json") for n in ("transform", "root_motion", "float")]]
    inputs = [p.read_bytes() for p in files]
    result = verify_generic_candidate(*[json.loads(b) for b in inputs])
    result["inputs"] = [{"path":str(p.resolve()), "sha256":hashlib.sha256(b).hexdigest()} for p,b in zip(files,inputs)]
    with args.output.open("x",encoding="utf-8") as stream:
        json.dump(result,stream,ensure_ascii=False,indent=2,allow_nan=False)
    print(json.dumps({k:v for k,v in result.items() if k not in ("tracks","inputs")}))


if __name__ == "__main__":
    main()
