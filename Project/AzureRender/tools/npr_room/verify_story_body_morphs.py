"""Compare the assembled body morph weights with direct Unity clip samples."""

import argparse
import gzip
import json
from pathlib import Path

import numpy as np

from tools.build_face_sdf import parse_glb
from tools.npr_room.build_story_signal_room import load
from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest
from tools.npr_room.story_body_morphs import FACE, MORPH_RECEIPT
from tools.npr_room.verify_timer_motion import accessor


def verify(models, original, samples):
    manifest = load(models / MORPH_RECEIPT)
    face = models / FACE
    assert manifest["faceGlbSHA256"] == digest(face)
    assert manifest["inputGlbSHA256"] == digest(original / FACE)
    base, baseline = parse_glb(original / FACE)
    model, binary = parse_glb(face)
    assert binary.startswith(baseline)
    assert all(base[name] == model[name] for name in ("meshes", "nodes", "skins", "materials"))
    mesh, = model["meshes"]
    with gzip.open(samples, "rt", encoding="utf-8-sig") as source:
        unity = json.load(source)
    assert mesh["extras"]["targetNames"] == unity["targets"]
    assert len(unity["targets"]) == manifest["targetCount"] == 57
    assert {clip["name"] for clip in unity["clips"]} == manifest["clips"].keys()
    expected_count = 0
    largest = 0
    for clip in unity["clips"]:
        record = manifest["clips"][clip["name"]]
        index = record["clipIndex"]
        animation = model["animations"][index]
        assert animation["name"] == clip["name"]
        prior = base["animations"][index]
        channels = [row for row in animation["channels"] if row["target"]["path"] == "weights"]
        assert [row for row in animation["channels"] if row not in channels] == prior["channels"]
        assert animation["samplers"][:len(prior["samplers"])] == prior["samplers"]
        assert len(channels) == bool(record["bindings"])
        times = np.array([frame["seconds"] for frame in clip["frames"]])
        expected = np.asarray([frame["weights"] for frame in clip["frames"]]) / 100
        if channels:
            sampler = animation["samplers"][channels[0]["sampler"]]
            keys = np.asarray(accessor(model, binary, sampler["input"], 1)).ravel()
            weights = np.asarray(accessor(model, binary, sampler["output"], 1)).reshape(-1, len(unity["targets"]))
            assert len(keys) == record["samples"] and keys[0] == times[0] == 0
            assert abs(keys[-1] - clip["duration"]) <= 1e-5
            actual = np.stack([np.interp(times, keys, weights[:, index]) for index in range(weights.shape[1])], axis=1)
        else:
            actual = np.asarray(mesh["weights"])
        error = float(np.max(np.abs(actual - expected)))
        assert error < 1e-5, (clip["name"], error)
        largest = max(largest, error)
        expected_count += expected.size
    return {"weights": expected_count, "clips": len(unity["clips"]), "maxWeightError": largest,
            "samplesSHA256": digest(samples), "faceGlbSHA256": digest(face), "unityVersion": unity["editor"]}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("models", "original", "samples", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    result = verify(args.models, args.original, args.samples)
    write(args.output, result)
    print(json.dumps(result))
