"""Verify paired original Windows room colors, depth and runtime state."""

import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image

from tools.npr_room.reference_inventory import digest


def verify_render_state(record, require_ambient_probe=False):
    required = ("lights", "renderers", "weatherScalars", "environment")
    if any(not record.get(key) for key in required):
        raise ValueError("Original render state is incomplete")
    transforms = {node["id"] for node in record["transforms"]}
    values = [record["environment"][key] for key in ("ambientIntensity", "reflectionIntensity")]
    materials = 0
    for kind in ("lights", "renderers"):
        rows = record[kind]
        if len({row["id"] for row in rows}) != len(rows) or any(row["transformId"] not in transforms for row in rows):
            raise ValueError("Original light or renderer transform is unbound")
        for row in rows:
            if kind == "lights":
                values.extend([row["intensity"], *row["color"].values()])
                if row["intensity"] < 0:
                    raise ValueError("Original light intensity is negative")
            else:
                for material in row["materials"]:
                    if not material["shader"]:
                        raise ValueError("Original renderer has an unresolved shader")
                    values.extend(item["value"] for item in material["floats"])
                    values.extend(component for item in material["colors"] for component in item["value"].values())
                    materials += 1
    values.extend(item["value"] for item in record["weatherScalars"])
    if not np.isfinite(values).all():
        raise ValueError("Original render state has nonfinite values")
    result = {"lights": len(record["lights"]), "renderers": len(record["renderers"]),
              "materials": materials, "weatherScalars": len(record["weatherScalars"])}
    if require_ambient_probe:
        environment = record["environment"]
        coefficients = environment.get("ambientProbe", [])
        samples = environment.get("ambientProbeSamples", [])
        if len(coefficients) != 27 or len(samples) != 26:
            raise ValueError("Original ambient probe requires 27 coefficients and 26 evaluated directions")
        directions = np.array([[sample["direction"][axis] for axis in "xyz"] for sample in samples])
        colors = np.array([[sample["value"][axis] for axis in "rgba"] for sample in samples])
        if (not np.isfinite(coefficients).all() or not np.isfinite(directions).all() or not np.isfinite(colors).all()
                or not np.allclose(np.linalg.norm(directions, axis=1), 1, atol=1e-6)
                or len({tuple(direction) for direction in directions}) != 26
                or any(type(environment.get(flag)) is not bool for flag in (
                    "lightsUseLinearIntensity", "lightsUseColorTemperature"))):
            raise ValueError("Original ambient probe state is invalid")
        result.update(ambientProbeCoefficients=27, ambientProbeDirections=26)
    return result


def verify_frames(folder, require_render_state=False, require_ambient_probe=False):
    paths = sorted(folder.glob("*.json"))
    expected = {f"{mode}-{index:02d}.json" for mode in ("Night", "Cloudy") for index in range(12)}
    if {path.name for path in paths} != expected:
        raise ValueError("Expected twelve original frames for each weather mode")
    report = {"schemaVersion": 1, "modes": {}}
    for mode in ("Night", "Cloudy"):
        frames = []
        colors = []
        depths = []
        records = []
        for index in range(12):
            stem = f"{mode}-{index:02d}"
            path = folder / (stem + ".json")
            record = json.loads(path.read_bytes())
            if (record["weather"], record["index"], record["color"], record["depth"]) != (
                    mode, index, stem + ".png", stem + "-depth.f32"):
                raise ValueError(f"Frame identity differs: {stem}")
            width, height = record["width"], record["height"]
            if (width, height) != (1280, 720) or record["depthGraphicsFormat"] != "R32_SFloat":
                raise ValueError(f"Unexpected source viewport or depth format: {stem}")
            depth_path = folder / record["depth"]
            if depth_path.stat().st_size != width * height * 4:
                raise ValueError(f"Depth buffer size differs: {stem}")
            depth = np.fromfile(depth_path, dtype="<f4").reshape(height, width)
            if not np.isfinite(depth).all() or depth.min() < 0 or depth.max() > 1 or np.unique(depth).size < 100:
                raise ValueError(f"Depth does not contain a finite geometry buffer: {stem}")
            with Image.open(folder / record["color"]) as image:
                if image.size != (width, height):
                    raise ValueError(f"Color size differs from its paired depth: {stem}")
                color = np.array(image.convert("RGB"))
            transforms = record["transforms"]
            identities = {node["id"] for node in transforms}
            parents = {node["parentId"] for node in transforms} - {0}
            if len(identities) != len(transforms) or not parents <= identities or not record["skins"]:
                raise ValueError(f"Runtime transform and skin state is incomplete: {stem}")
            pose = np.array([[*node["position"].values(), *node["rotation"].values(), *node["scale"].values()]
                             for node in transforms])
            if not np.isfinite(pose).all() or not record["shaderValues"]:
                raise ValueError(f"Runtime pose or weather values are incomplete: {stem}")
            if not 0 < record["near"] < record["far"] or not record["camera"]:
                raise ValueError(f"Missing camera projection: {stem}")
            frames.append({"metadata": path.name, "metadataSHA256": digest(path),
                           "colorSHA256": digest(folder / record["color"]), "depthSHA256": digest(depth_path),
                           "frame": record["frame"], "simulationTime": record["simulationTime"],
                           "depthRange": [float(depth.min()), float(depth.max())],
                           "transforms": len(transforms), "skins": len(record["skins"]),
                           "shaderValues": len(record["shaderValues"])})
            if require_render_state or require_ambient_probe:
                frames[-1]["renderState"] = verify_render_state(record, require_ambient_probe)
            colors.append(color)
            depths.append(depth)
            records.append(record)
        numbers = np.array([row["frame"] for row in frames])
        timestamps = np.array([row["simulationTime"] for row in frames])
        if not np.all(np.diff(numbers) == 4) or not np.allclose(np.diff(timestamps), 4 / 60, atol=0.00001):
            raise ValueError(f"Source frame interval differs: {mode}")
        changed = [int(np.count_nonzero(np.any(color != colors[0], axis=2))) for color in colors[1:]]
        depth_changed = [int(np.count_nonzero(depth != depths[0])) for depth in depths[1:]]
        if not max(changed) or not max(depth_changed):
            raise ValueError(f"The room sequence has no observed color or depth dynamics: {mode}")
        first = records[0]
        report["modes"][mode] = {
            "frames": frames, "intervalSeconds": 4 / 60, "colorChangesFromFirst": changed,
            "depthChangesFromFirst": depth_changed, "firstWeatherClock": [
                first["clockHours"], first["clockMinutes"], first["clockSeconds"], first["clockMilliseconds"]],
            "firstCamera": {"position": first["position"], "rotation": first["rotation"],
                            "projection": first["projection"], "worldToCamera": first["worldToCamera"]},
            "cameraPositionStable": all(row["position"] == first["position"] for row in records),
            "cameraRotationStable": all(row["rotation"] == first["rotation"] for row in records),
            "atmosphereProfiles": sorted({row["atmosphereProfile"] for row in records}),
        }
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--frames", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--require-render-state", action="store_true")
    parser.add_argument("--require-ambient-probe", action="store_true")
    args = parser.parse_args()
    report = verify_frames(args.frames, args.require_render_state, args.require_ambient_probe)
    with args.output.open("x", encoding="utf-8", newline="\n") as output:
        json.dump(report, output, indent=2, sort_keys=True)
        output.write("\n")


if __name__ == "__main__":
    main()
