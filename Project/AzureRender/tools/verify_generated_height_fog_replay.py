"""Run the frozen height-fog replay fixtures and check Player frame output."""

import argparse
from hashlib import sha256
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

import numpy as np
from PIL import Image

from prepare_generated_height_fog_replay import RECIPES, json_bytes
from test_scene_fog_gpu import CAMERA_REENTRY_OFFSETS, frame_errors

FOG_SHADERS = (
    "view_fog_surface.frag", "view_fog_surface_samples.frag",
    "mesh_fog.frag", "mesh_fog_bindless.frag",
    "mesh_fog_samples.frag", "mesh_fog_bindless_samples.frag",
)


def digest(path):
    return sha256(path.read_bytes()).hexdigest()


def read(path):
    return json.loads(path.read_bytes())


def verify_inputs(frozen, prepared, manifest):
    for key, root in (("sourceSHA256", frozen), ("outputSHA256", prepared)):
        for relative, expected in manifest[key].items():
            path = (root / relative).resolve(strict=True)
            if not path.is_relative_to(root) or digest(path) != expected:
                raise ValueError(f"Frozen fixture differs: {path}")


def capture(player, project, recipe, root, fixed, environment, actions=None):
    root.mkdir()
    frames = root / "frames"
    width, height = map(str, recipe["extent"])
    count = recipe["frameCount"] if actions else 1
    command = [str(player), "--project", str(project), "--capture-dir", str(frames),
               "--capture-frames", str(count), "--capture-fps", str(recipe["fps"]),
               "--fixed-frame-step", "--qa-camera", "scene", "--qa-light", "scene",
               "--width", width, "--height", height]
    if fixed:
        command.extend(("--disable-bindless", "--disable-compute-skinning", "--disable-parallel-recording"))
    runtime = root / "runtime.json"
    if actions:
        command.extend(("--game-actions", str(actions), "--runtime-report", str(runtime)))
    (root / "command.json").write_bytes(json_bytes(command))
    log_file = root / "player.log"
    with log_file.open("w", encoding="utf-8") as output:
        subprocess.run(command, cwd=root, env=environment, stdout=output, stderr=subprocess.STDOUT,
                       text=True, check=True, timeout=120)
    log = log_file.read_text(encoding="utf-8", errors="replace")
    assert "Allocator after unload: buffers=0 images=0" in log, root
    assert ("Validation layer: enabled" in log or
            re.search(r'Insert instance layer "VK_LAYER_KHRONOS_validation"', log)), root
    assert not re.search(r"VUID-|SYNC-HAZARD|Validation (?:Error|Warning)|\[error\]", log), root
    expected_access = "fixed" if fixed else "indexed"
    assert f"Character texture access: {expected_access}," in log, root
    pixels = []
    for index in range(count):
        with Image.open(frames / f"frame_{index:06}.png") as image:
            assert list(image.size) == recipe["extent"], (root, image.size)
            pixels.append(np.array(image.convert("RGB")))
    return read(frames / "capture_manifest.json"), read(runtime) if actions else None, pixels


def verify_binding(report, binding, samples):
    generated = binding.get("generatedHeightVolume")
    assert report["viewFogMatch"] == ("generated-height" if generated else "analytic-only"), report["viewFogMatch"]
    assert report["viewFogEnabled"] is True and report["viewFogAnalyticEnabled"] is True
    assert report["viewFogFrameBytes"] == 336 and report["viewFogFrameLayoutVersion"] == 2
    assert report.get("viewFogGeneratedHeightEnabled", False) is bool(generated)
    if generated:
        assert report["viewFogGeneratedHeightDimensions"] == generated["dimensions"]
        assert report["viewFogGeneratedHeightFarViewDepth"] == generated["farViewDepth"]
    assert report["snapshotRasterizationSamples"] == report["rasterizationSamples"] == samples
    if samples == 4:
        assert report["snapshotMultisampleBytes"] > 0, "Four-sample fog requires multisample snapshot storage"


def run_case(player, frozen, prepared, output, case, recipe, mode, fixed, environment, samples=1):
    access = "fixed" if fixed else "indexed"
    root = output / f"{case}-{mode}-{access}-{samples}x"
    fixture = root / "fixture"
    shutil.copytree(prepared / case / mode, fixture)
    fixtures = RECIPES[case]
    levels = fixtures["levels"]
    bindings = {}
    views = {}
    for level in levels:
        path = fixture / "assets" / level
        scene = read(path)
        if samples != 1:
            scene["renderSettings"]["antiAliasing"] = samples
            path.write_bytes(json_bytes(scene))
        bindings[level] = scene["viewFog"]
        views[level] = read(frozen / fixtures["source"] / "assets" / level)["viewFog"]["captureView"]
    project = fixture / recipe["project"]
    report, runtime, frames = capture(player, project, recipe, root / "replay", fixed, environment,
                                      fixture / recipe["actions"])
    verify_binding(report, bindings[levels[0]], samples)
    assert not runtime["scriptErrors"] and not runtime["presentationErrors"], root
    trace = runtime["routeFrames"]
    assert len(trace) == len(frames) == recipe["frameCount"], root
    assert np.array_equal(frames[0], frames[-1]), root
    references = {}
    if case == "roundtrip":
        for level in levels:
            snapshot = fixture / f"{Path(level).stem}-snapshot.azureproject"
            snapshot.write_bytes(json_bytes(dict(read(project), startupScene=f"assets:/{level}")))
            snapshot_report, _, images = capture(player, snapshot, recipe, root / f"snapshot-{Path(level).stem}",
                                                  fixed, environment)
            verify_binding(snapshot_report, bindings[level], samples)
            references[level] = images[0]

    transitions, frame_hashes, errors = [], [], []
    for index, (frame, observation) in enumerate(zip(frames, trace)):
        level = Path(observation["levelReference"]).name
        assert not observation["loadError"] and level in views, (root, index, observation)
        if not transitions or transitions[-1]["level"] != level:
            transitions.append({"level": level, "frame": index})
        view = views[level]
        if case == "motion":
            x, y = CAMERA_REENTRY_OFFSETS[min(max(index - 1, 0), len(CAMERA_REENTRY_OFFSETS) - 1)]
            position, target = [x, y, 3.], [x, y, 2.]
        else:
            position, target = view["position"], view["target"]
            assert np.array_equal(frame, references[level]), (root, index, level)
        assert np.allclose(observation["camera"], position, rtol=0, atol=1e-6), (root, index, observation)
        assert np.allclose(observation["target"], target, rtol=0, atol=1e-6), (root, index, observation)
        errors.append(frame_errors(frame, "camera-reentry" if case == "motion" else "height-only",
                                   view, np.array(observation["camera"]), False))
        frame_hashes.append(sha256(frame.tobytes()).hexdigest())
    expected = list(levels) if case == "motion" else [levels[0], levels[1], levels[0]]
    assert [row["level"] for row in transitions] == expected and len(set(frame_hashes)) > 1, root
    return {"frames": len(trace), "maximumLinearError": max(errors),
            "fogBinding": report["viewFogMatch"], "rasterizationSamples": report["rasterizationSamples"],
            "snapshotMultisampleBytes": report["snapshotMultisampleBytes"],
            "frameSHA256": frame_hashes, "transitions": transitions,
            "levelSnapshotSHA256": {level: sha256(image.tobytes()).hexdigest()
                                    for level, image in references.items()}}


def compare_paths(case, indexed, fixed):
    if case == "motion":
        assert indexed["frameSHA256"] == fixed["frameSHA256"], case
    else:
        # Level preparation runs asynchronously. Each replay frame has already
        # been matched to its committed level's independently loaded snapshot.
        assert indexed["levelSnapshotSHA256"] == fixed["levelSnapshotSHA256"], case


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("player", "frozen-run", "prepared", "output"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    parser.add_argument("--resource-root", type=Path)
    parser.add_argument("--mode", choices=("analytic", "generated", "both"), default="both")
    args = parser.parse_args()
    frozen, prepared = args.frozen_run.resolve(strict=True), args.prepared.resolve(strict=True)
    output, player = args.output.resolve(), args.player.resolve(strict=True)
    if any(output == path or path in output.parents for path in (frozen, prepared)):
        parser.error("Output must be outside the read-only fixture inputs")
    manifest_path = prepared / "replay-manifest.json"
    manifest = read(manifest_path)
    assert manifest["schemaVersion"] == 1
    expected_cases = {case: {"project": recipe["project"], "actions": recipe["actions"],
                             "frameCount": recipe["frames"], "fps": 60, "extent": [128, 128]}
                      for case, recipe in RECIPES.items()}
    assert manifest["cases"] == expected_cases, "Unexpected frozen replay recipe"
    verify_inputs(frozen, prepared, manifest)
    output.mkdir()
    resource_root = (args.resource_root or player.parent).resolve(strict=True)
    shaders = FOG_SHADERS + (("view_fog_generate.comp",) if args.mode != "analytic" else ())
    paths = [player, manifest_path, *(resource_root / "shaders" / f"{shader}.spv" for shader in shaders)]
    fingerprints = {path: digest(path) for path in paths}
    environment = dict(os.environ, AZURERENDER_RESOURCE_ROOT=str(resource_root),
                       VK_INSTANCE_LAYERS="VK_LAYER_KHRONOS_validation",
                       VK_LAYER_VALIDATE_SYNC="1", VK_LOADER_DEBUG="error,layer")
    cases = {}
    modes = ("analytic", "generated") if args.mode == "both" else (args.mode,)
    for mode in modes:
        for case, recipe in manifest["cases"].items():
            for samples in ((1, 4) if case == "motion" else (1,)):
                indexed, fixed = [run_case(player, frozen, prepared, output, case, recipe, mode, fixed,
                                           environment, samples) for fixed in (False, True)]
                compare_paths(case, indexed, fixed)
                cases[f"{case}-{mode}-{samples}x"] = {"indexed": indexed, "fixed": fixed}
                print(case, mode, samples, max(indexed["maximumLinearError"], fixed["maximumLinearError"]),
                      flush=True)
    verify_inputs(frozen, prepared, manifest)
    assert all(digest(path) == fingerprint for path, fingerprint in fingerprints.items()), "Replay programs changed"
    (output / "verification.json").write_bytes(json_bytes({
        "schemaVersion": 1, "status": "passed", "mode": args.mode, "cases": cases, "budget": .008,
        "playerSHA256": fingerprints[player], "fixtureSHA256": fingerprints[manifest_path],
        "shaderSHA256": {path.name: fingerprints[path] for path in paths[2:]}, "descriptorExact": True,
    }))


if __name__ == "__main__":
    main()
