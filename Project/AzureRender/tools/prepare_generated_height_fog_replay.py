"""Prepare portable analytic controls and generated height-fog replay inputs."""

import argparse
from hashlib import sha256
import json
from pathlib import Path
import tempfile

from test_scene_fog_gpu import CAMERA_REENTRY_FRAMES, analytic_settings


RECIPES = {
    "motion": {
        "source": "full-fog-fixture/indexed-camera-reentry",
        "project": "project.azureproject",
        "actions": "route.json",
        "levels": ("scene.azurelevel",),
        "assets": ("surface.gltf", "rear.gltf", "camera.lua"),
        "frames": CAMERA_REENTRY_FRAMES,
        "sourceVersion": 2,
    },
    "roundtrip": {
        "source": "fog-baked-height-route",
        "project": "route.azureproject",
        "actions": "actions.json",
        "levels": ("base.azurelevel", "moved.azurelevel"),
        "assets": ("surface.gltf", "rear.gltf"),
        "frames": 72,
        "sourceVersion": 1,
    },
}


def json_bytes(value):
    return (json.dumps(value, indent=2, sort_keys=True, ensure_ascii=False) + "\n").encode("utf-8")


def prepare(frozen, output):
    frozen = frozen.resolve(strict=True)
    output = output.resolve()
    if output.exists():
        raise FileExistsError(output)
    if frozen in output.parents:
        raise ValueError("Replay output must be separate from its frozen inputs")

    source_hashes = {}
    outputs = {}
    cases = {}

    def source_file(relative):
        data = (frozen / relative).read_bytes()
        source_hashes[relative] = sha256(data).hexdigest()
        return data

    for case, recipe in RECIPES.items():
        source = recipe["source"]
        paths = [recipe["project"], recipe["actions"]]
        for name in (*recipe["levels"], *recipe["assets"]):
            paths.extend((f"assets/{name}", f"assets/{name}.azmeta"))
        shared = {relative: source_file(f"{source}/{relative}") for relative in paths}
        levels = {}
        for name in recipe["levels"]:
            relative = f"assets/{name}"
            level = json.loads(shared.pop(relative))
            binding = level["viewFog"]
            if binding["schemaVersion"] != recipe["sourceVersion"] or binding["enabled"] is not True:
                raise ValueError(f"Unexpected frozen fog binding in {relative}")
            binding.update(schemaVersion=2, asset="", captureView=None)
            if recipe["sourceVersion"] == 1:
                binding["analytic"] = analytic_settings("height-only")
            levels[relative] = level

        cases[case] = {"project": recipe["project"], "actions": recipe["actions"],
                       "frameCount": recipe["frames"], "fps": 60, "extent": [128, 128]}
        for mode in ("analytic", "generated"):
            prefix = f"{case}/{mode}"
            outputs.update({f"{prefix}/{relative}": data for relative, data in shared.items()})
            for relative, level in levels.items():
                scene = level
                if mode == "generated":
                    binding = dict(level["viewFog"], schemaVersion=3, generatedHeightVolume={
                        "dimensions": [48, 32, 48], "farViewDepth": 8.0,
                    })
                    scene = dict(level, viewFog=binding)
                outputs[f"{prefix}/{relative}"] = json_bytes(scene)

    manifest = {"schemaVersion": 1, "cases": cases, "sourceSHA256": source_hashes,
                "outputSHA256": {relative: sha256(data).hexdigest() for relative, data in outputs.items()}}
    outputs["replay-manifest.json"] = json_bytes(manifest)
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="fog-replay-", dir=output.parent) as temporary:
        stage = Path(temporary) / "prepared"
        stage.mkdir()
        for relative, data in outputs.items():
            path = stage / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        stage.rename(output)
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--frozen-run", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    manifest = prepare(args.frozen_run, args.output)
    print(json.dumps({"cases": list(manifest["cases"]), "files": len(manifest["outputSHA256"])}))


if __name__ == "__main__":
    main()
