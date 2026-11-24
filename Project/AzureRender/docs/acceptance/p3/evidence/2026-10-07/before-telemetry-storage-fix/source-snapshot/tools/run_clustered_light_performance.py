#!/usr/bin/env python3
"""Measures clustered-light GPU timings over a deterministic light-count sweep."""

import argparse
import json
import re
from datetime import datetime, timezone
from pathlib import Path

from run_visual_regression import run_renderer

PROJECT_ROOT = Path(__file__).resolve().parent.parent


def scene_text(light_count: int) -> str:
    lines = [
        "AzureRender Scene v1",
        "schemaVersion 3",
        "renderSettingsVersion 7",
        f'sceneId "clustered-light-scale-{light_count}"',
        "resourceCount 1",
        'resource "asset-0" "gltf" "assets_public/test_model.gltf"',
        f"nodeCount {light_count + 1}",
        'node "root" "Public character" "" "asset-0"',
        "visible true",
        "transform 0 0 0 0 0 0 1 1 1",
        'prefab "" ""',
    ]
    for index in range(light_count):
        node_id = f"light-node-{index:03d}"
        x = -0.9 + (index % 8) * (1.8 / 7.0)
        y = -0.75 + ((index // 8) % 4) * 0.5
        z = -0.75 + (index // 32) * 0.5
        lines.extend([
            f'node "{node_id}" "Scale light {index:03d}" "" ""',
            "visible false",
            "transform " + " ".join(f"{value:.6f}" for value in (
                x, y, z, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)),
            'prefab "" ""',
        ])
    lines.append(f"lightCount {light_count}")
    for index in range(light_count):
        channel = index % 3
        colors = ((1.0, 0.42, 0.28), (0.32, 0.70, 1.0), (0.42, 1.0, 0.48))
        color = colors[channel]
        lines.append(
            f'light "light-{index:03d}" "light-node-{index:03d}" '
            f"{color[0]:.2f} {color[1]:.2f} {color[2]:.2f} 1.2 0.65 true")
    lines.extend([
        "showcasePreset 0",
        "styleMaskStrength 1",
        "diffuseBandThreshold 0.4",
        "shadowMaximumFilterRadiusTexels 8",
        "innerOutlineEnabled true",
        "outlineStrength 0.4",
        "gradeExposureEv 0",
        "characterBackgroundEnabled true",
        "characterPlatformEnabled true",
        "blackholeQuality cinematic",
        "blackholeCamera front",
        "sceneRenderer character",
        "",
    ])
    return "\n".join(lines)


def run_case(
    executable: Path,
    output_dir: Path,
    light_count: int,
    frames: int,
) -> dict:
    scene_path = output_dir / "scenes" / f"lights_{light_count:03d}.azscene"
    report_path = output_dir / f"lights_{light_count:03d}.json"
    scene_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.unlink(missing_ok=True)
    scene_path.write_text(scene_text(light_count), encoding="utf-8")
    completed = run_renderer([
        str(executable),
        "--scene", str(scene_path),
        "--width", "1280",
        "--height", "720",
        "--smoke-frames", str(frames),
        "--gpu-timing-output", str(report_path),
    ])
    output = completed.stdout + "\n" + completed.stderr
    if completed.returncode != 0:
        raise RuntimeError(
            f"{light_count} lights failed: "
            f"{completed.stderr.strip() or completed.stdout.strip()}")
    if not report_path.is_file():
        raise RuntimeError(f"{light_count} lights produced no GPU timing report")
    matched = re.search(
        r"Clustered lighting: (\d+) lights, (\d+) cluster references", output)
    if matched is None or int(matched.group(1)) != light_count:
        raise RuntimeError(f"{light_count} light entities did not reach the GPU upload")
    report = json.loads(report_path.read_text(encoding="utf-8"))
    return {
        "lightCount": light_count,
        "clusterReferences": int(matched.group(2)),
        "samples": report["samples"],
        "gpu": report["gpu"],
        "shadowAverageMs": report["shadowAverageMs"],
        "mainSceneAverageMs": report["mainSceneAverageMs"],
        "postProcessAverageMs": report["internalOutlineAverageMs"],
        "totalAverageMs": report["totalAverageMs"],
        "totalP95Ms": report["totalP95Ms"],
        "totalP99Ms": report["totalP99Ms"],
        "drawCallsPerFrame": report["submission"]["drawCallsPerFrame"],
        "descriptorSetBindsPerFrame": report["submission"][
            "descriptorSetBindsPerFrame"],
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--frames", type=int, default=300)
    parser.add_argument(
        "--light-counts", type=int, nargs="+", default=[0, 16, 64, 128])
    args = parser.parse_args()
    if any(count < 0 or count > 128 for count in args.light_counts):
        parser.error("light counts must be within 0..128")

    results = []
    try:
        for light_count in args.light_counts:
            results.append(run_case(
                args.executable, args.output_dir, light_count, args.frames))
    except RuntimeError as error:
        print(error)
        return 1
    summary = {
        "schemaVersion": 1,
        "capturedAtUtc": datetime.now(timezone.utc).isoformat(
            timespec="seconds"),
        "width": 1280,
        "height": 720,
        "frames": args.frames,
        "lightRadius": 0.65,
        "results": results,
    }
    args.output_dir.mkdir(parents=True, exist_ok=True)
    summary_path = args.output_dir / "clustered-light-performance.json"
    summary_path.write_text(
        json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    for result in results:
        print(
            f"lights={result['lightCount']:3d} "
            f"references={result['clusterReferences']:6d} "
            f"main={result['mainSceneAverageMs']:.3f}ms "
            f"total={result['totalAverageMs']:.3f}ms "
            f"p95={result['totalP95Ms']:.3f}ms")
    print(f"summary={summary_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
