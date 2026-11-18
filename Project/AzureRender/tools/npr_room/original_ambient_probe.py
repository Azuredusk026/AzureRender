"""Transfer the original evaluated ambient SH response to a quadratic grid."""

import argparse
import json
import math
from pathlib import Path
from struct import pack

import numpy as np

from tools.npr_room.original_room_frames import verify_render_state
from tools.npr_room.reference_inventory import digest


def fit_probe(samples):
    if len(samples) != 26:
        raise ValueError("Original ambient probe requires all 26 evaluated directions")
    directions = np.array([[row["direction"][axis] for axis in "xyz"] for row in samples], dtype=float)
    colors = np.array([[row["value"][axis] for axis in "rgb"] for row in samples], dtype=float)
    if (not np.isfinite(directions).all() or not np.isfinite(colors).all()
            or not np.allclose(np.linalg.norm(directions, axis=1), 1, atol=1e-6)):
        raise ValueError("Ambient probe directions and colors must be finite and normalized")
    directions[:, 2] *= -1
    x, y, z = directions.T
    basis = np.column_stack((x, y, z, np.ones(len(x)), x*y, y*z, z*z, x*z, x*x-y*y))
    coefficients, _, rank, _ = np.linalg.lstsq(basis, colors, rcond=None)
    error = float(np.max(np.abs(basis @ coefficients - colors)))
    if rank != 9 or not np.isfinite(coefficients).all() or not math.isfinite(error) or error > 1e-5:
        raise ValueError("Original ambient response is outside the quadratic representation")
    return {"linear": coefficients[:4].T.tolist(), "quadratic": coefficients[4:].T.tolist(), "maximumError": error}


def encode(probe, origin, spacing):
    if len(origin) != 3 or not all(math.isfinite(value) for value in (*origin, spacing)) or spacing <= 0:
        raise ValueError("Ambient grid requires a finite origin and positive spacing")
    linear = np.asarray(probe["linear"], dtype=float)
    quadratic = np.asarray(probe["quadratic"], dtype=float)
    if linear.shape != (3, 4) or quadratic.shape != (3, 5):
        raise ValueError("Ambient grid requires three complete linear and quadratic coefficient rows")
    values = np.concatenate((linear.ravel(), quadratic.ravel(), origin, [spacing], np.asarray(origin) + spacing))
    if not np.isfinite(values).all() or np.any(np.abs(values) > np.finfo(np.float32).max):
        raise ValueError("Ambient grid values exceed the finite single-precision range")
    cell = bytearray()
    for linear, quadratic in zip(probe["linear"], probe["quadratic"], strict=True):
        cell.extend(pack("<8f", *linear, *quadratic[:4]))
    cell.extend(pack("<4f", *(row[4] for row in probe["quadratic"]), 1))
    cell.extend(bytes(16))
    return b"AZIV0002" + pack("<4f3I", *origin, spacing, 2, 2, 2) + bytes(cell) * 8


def convert(frame, output, origin=(-2048., -2048., -2048.), spacing=4096.):
    record = json.loads(frame.read_bytes())
    verify_render_state(record, require_ambient_probe=True)
    probe = fit_probe(record["environment"]["ambientProbeSamples"])
    payload = encode(probe, origin, spacing)
    with output.open("xb") as stream:
        stream.write(payload)
    return {"schemaVersion": 1, "sourceSHA256": digest(frame), "volumeSHA256": digest(output),
            "weather": record["weather"], "frame": record["frame"], "origin": list(origin), "spacing": spacing,
            "linear": probe["linear"], "quadratic": probe["quadratic"], "maximumFittingError": probe["maximumError"]}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--frame", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--origin", type=float, nargs=3, default=(-2048., -2048., -2048.))
    parser.add_argument("--spacing", type=float, default=4096.)
    args = parser.parse_args()
    receipt_path = args.output.with_name(args.output.name + ".source.json")
    if receipt_path.exists():
        raise FileExistsError(receipt_path)
    result = convert(args.frame, args.output, args.origin, args.spacing)
    with receipt_path.open("x", encoding="utf-8") as receipt:
        json.dump(result, receipt, indent=2, sort_keys=True)
        receipt.write("\n")
