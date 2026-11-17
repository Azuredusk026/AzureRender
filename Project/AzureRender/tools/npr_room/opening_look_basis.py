"""Calibrate the source Avatar's local swing and twist and verify the look offset."""

import argparse
import gzip
import json
import math
from pathlib import Path

import numpy as np

from tools.npr_room.build_story_signal_room import load
from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest
from tools.npr_room.skinned_mesh_export import UnityScene, read_morph_curves, sample_shape
from tools.npr_room.verify_timer_motion import angle

MIRROR = np.array([-1, -1, 1])


def quaternion(value):
    result = np.array([value[key] for key in "xyzw"], dtype=float)
    return unit(result)


def unit(value):
    length = np.linalg.norm(value)
    if not length or not math.isfinite(length):
        raise ValueError("Invalid source rotation basis")
    return value / length


def conjugate(q):
    return q * [-1, -1, -1, 1]


def product(a, b):
    return np.r_[a[3]*b[:3] + b[3]*a[:3] + np.cross(a[:3], b[:3]), a[3]*b[3] - np.dot(a[:3], b[:3])]


def rotation_vector(q):
    q = unit(q)
    if q[3] < 0:
        q = -q
    sine = np.linalg.norm(q[:3])
    return q[:3] * (2 * math.atan2(sine, q[3]) / sine) if sine else np.zeros(3)


def calibrate(samples):
    positions = {tuple(round(x, 3) for x in row["muscle"]): quaternion(row["rotation"]) for row in samples}
    zero = positions[(0, 0, 0)]
    axes, rates = [], []
    for coordinate in range(3):
        negative = [0] * 3
        negative[coordinate] = -.8
        positive = [0] * 3
        positive[coordinate] = .8
        before = rotation_vector(product(conjugate(zero), positions[tuple(negative)])) / -.8
        after = rotation_vector(product(conjugate(zero), positions[tuple(positive)])) / .8
        axis = unit(after)
        if np.linalg.norm(unit(before) - axis) > 1e-5:
            raise ValueError("Positive and negative source limits do not share an axis")
        axes.append(axis)
        rates.append([float(np.linalg.norm(before)), float(np.linalg.norm(after))])
    if np.max(np.abs(np.asarray(axes) @ np.asarray(axes).T - np.eye(3))) >= 1e-5:
        raise ValueError("The source Avatar swing and twist axes must be orthogonal")
    return zero, axes, rates


def encode(muscles, basis):
    zero, axes, rates = basis
    x, y, twist = (value * limits[value >= 0] for value, limits in zip(muscles, rates, strict=True))
    swing = unit(np.r_[math.tan(x/2)*axes[0] + math.tan(y/2)*axes[1], 1.])
    along = np.r_[math.sin(twist/2)*axes[2], math.cos(twist/2)]
    return unit(product(zero, product(swing, along)))


def decode(rotation, basis):
    zero, axes, rates = basis
    q = product(conjugate(zero), rotation)
    twist = unit(np.r_[np.dot(q[:3], axes[2])*axes[2], q[3]])
    swing = product(q, conjugate(twist))
    if twist[3] < 0:
        twist = -twist
    angles = [2 * math.atan2(np.dot(swing[:3], axis), swing[3]) for axis in axes[:2]]
    angles.append(2 * math.atan2(np.dot(twist[:3], axes[2]), twist[3]))
    return [value / limits[value >= 0] for value, limits in zip(angles, rates, strict=True)]


def source_overlay(path, receipt):
    inputs = load(receipt)
    relative, = [name for name in inputs["assets"] if Path(name).stem == "anim_lookplayer_start"]
    clip = path / relative
    if digest(clip) != inputs["assets"][relative]:
        raise ValueError("Changed source gaze clip")
    data = UnityScene(clip).read(7400000)
    settings = data["m_AnimationClipSettings"]
    assert settings["m_StartTime"] == 0 and not settings["m_LoopTime"] and not settings["m_HasAdditiveReferencePose"]
    length = settings["m_StopTime"]
    curves = {}
    for binding in data["m_FloatCurves"]:
        if binding["classID"] != 95:
            continue
        keys = binding["curve"]["m_Curve"]
        assert not any(row["weightedMode"] for row in keys)
        curves[binding["attribute"]] = ([row["time"] for row in keys], [row["value"] for row in keys],
                                         [(float(row["inSlope"]), float(row["outSlope"])) for row in keys])
    eyes = read_morph_curves([row for row in data["m_FloatCurves"] if row["classID"] == 137],
                             "Character/Face", ["blendShape1.Eye_look_left", "blendShape1.Eye_look_right"], 0)
    return digest(clip), length, curves, eyes


def mirrored(q):
    return [*(q[:3] * MIRROR), q[3]]


def verify(source, receipt, basis_file, additive_file, native_fixture=None):
    with gzip.open(basis_file, "rt", encoding="utf-8-sig") as stream:
        ref = json.load(stream)
    with gzip.open(additive_file, "rt", encoding="utf-8-sig") as stream:
        overlay = json.load(stream)
    sha, length, curves, eyes = source_overlay(source, receipt)
    model, native, basis_worst, animation_worst, eyes_worst, pose_count = [], [], 0., 0., 0., 0
    for index, joint in enumerate(ref["basis"]):
        basis = calibrate(joint["samples"])
        for row in joint["samples"]:
            basis_worst = max(basis_worst, angle(encode(row["muscle"], basis), quaternion(row["rotation"])))
        zero, axes, rates = basis
        model.append({"joint": joint["joint"], "zero": mirrored(zero),
                      "axes": [(axis*MIRROR).tolist() for axis in axes], "rates": rates, "muscles": joint["muscles"]})
        native_cases = []
        for recorded, other in zip(ref["animated"], overlay["frames"], strict=True):
            raw = quaternion(recorded["normal"][index]["rotation"])
            origin = decode(raw, basis)
            for view in other["look"]:
                time = view["scale"] * length
                added = [sample_shape(*curves[name], time) for name in joint["muscles"]]
                computed = encode([a+b for a, b in zip(origin, added, strict=True)], basis)
                delta = quaternion(view["rotations"][overlay["joints"].index(joint["joint"])])
                target = product(raw, delta)
                if math.isclose(view["scale"], .7, abs_tol=1e-6):
                    assert angle(target, quaternion(recorded["gaze"][index]["rotation"])) < 1e-5
                animation_worst = max(animation_worst, angle(computed, target))
                pose_count += 1
                if native_fixture:
                    native_cases.append({"base": mirrored(raw), "offset": added, "expected": mirrored(target)})
        if native_fixture:
            native.append({"basis": {key: model[-1][key] for key in ("zero", "axes", "rates")},
                           "samples": native_cases})
    for frame in overlay["frames"]:
        for row in frame["look"]:
            for side in ("left", "right"):
                expected = sample_shape(*eyes["blendShape1.Eye_look_" + side], row["scale"] * length)
                eyes_worst = max(eyes_worst, abs(expected - row[side] / 100))
    assert basis_worst < .001 and animation_worst < .001 and eyes_worst < 1e-6, (basis_worst, animation_worst, eyes_worst)
    if native_fixture:
        write(native_fixture, {"schemaVersion": 1, "joints": native})
    return {"schemaVersion": 1, "clipSHA256": sha, "basisSHA256": digest(basis_file), "referenceSHA256": digest(additive_file),
            "unityVersion": ref["editor"], "calibrationPoses": sum(len(row["samples"]) for row in ref["basis"]),
            "checkedRotations": pose_count, "maxCalibrationDegrees": basis_worst, "maxRotationDegrees": animation_worst,
            "maxEyeWeightError": eyes_worst, "jointBasis": model}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    names = ("source", "receipt", "basis", "additive", "output")
    for name in names:
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--native-fixture", type=Path)
    args = vars(parser.parse_args())
    paths = [args[name].resolve() for name in names]
    evidence = verify(*paths[:-1], native_fixture=args["native_fixture"])
    write(paths[-1], evidence)
    print(json.dumps({key: value for key, value in evidence.items() if key != "jointBasis"}))
