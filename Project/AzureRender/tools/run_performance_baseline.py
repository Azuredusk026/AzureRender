#!/usr/bin/env python3
"""Captures the performance baseline used to judge structural changes.

Runs a fixed set of scenes at a fixed resolution and frame count, then merges
the per-run GPU timing reports into one summary with a stable schema. Every
engine-evolution stage publishes a report in this format so numbers can be
compared directly across stages.

The counters matter as much as the timings: draw calls and descriptor binds per
frame are the evidence that a batching or bindless change actually reduced
CPU-side submission cost.
"""

import argparse
import json
import platform
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent

SCENES = [
    {"name": "character", "args": []},
    {"name": "blackhole", "args": ["--blackhole-quality", "balanced"]},
]

# `sample` is intentionally excluded. It records no scene timestamps, and the
# host reads the timestamp pool with VK_QUERY_RESULT_WAIT_BIT, so requesting GPU
# timing for it blocks indefinitely. The scene is still covered by the smoke
# and visual gates; only this timing baseline skips it.


def run_scene(
    executable: Path,
    scene: dict,
    width: int,
    height: int,
    frames: int,
    output_dir: Path,
) -> dict:
    report_path = output_dir / f"{scene['name']}.json"
    report_path.parent.mkdir(parents=True, exist_ok=True)
    # The renderer refuses to overwrite an existing timing report, so a rerun
    # of the same stage clears the previous one first.
    for stale in (report_path, report_path.with_suffix(".csv")):
        if stale.exists():
            stale.unlink()
    command = [
        str(executable),
        "--scene-type", scene["name"],
        "--width", str(width),
        "--height", str(height),
        "--smoke-frames", str(frames),
        "--gpu-timing",
        "--gpu-timing-output", str(report_path),
        *scene["args"],
    ]
    completed = subprocess.run(
        command, cwd=PROJECT_ROOT, capture_output=True, text=True)
    if completed.returncode != 0:
        return {
            "scene": scene["name"],
            "status": "failed",
            "exitCode": completed.returncode,
            "stderr": completed.stderr.strip()[-2000:],
        }
    if not report_path.is_file():
        return {
            "scene": scene["name"],
            "status": "failed",
            "reason": "no timing report produced",
        }

    report = json.loads(report_path.read_text(encoding="utf-8"))
    submission = report.get("submission", {})
    return {
        "scene": scene["name"],
        "status": "ok",
        "samples": report.get("samples"),
        "gpuMs": {
            "shadowAverage": report.get("shadowAverageMs"),
            "mainSceneAverage": report.get("mainSceneAverageMs"),
            "postProcessAverage": report.get("internalOutlineAverageMs"),
            "totalAverage": report.get("totalAverageMs"),
            "totalMin": report.get("totalMinMs"),
            "totalMax": report.get("totalMaxMs"),
            "totalP50": report.get("totalP50Ms"),
            "totalP95": report.get("totalP95Ms"),
            "totalP99": report.get("totalP99Ms"),
        },
        "submission": {
            "frames": submission.get("frames"),
            "drawCalls": submission.get("drawCalls"),
            "descriptorSetBinds": submission.get("descriptorSetBinds"),
            "pipelineBinds": submission.get("pipelineBinds"),
            "pushConstantUpdates": submission.get("pushConstantUpdates"),
            "drawCallsPerFrame": submission.get("drawCallsPerFrame"),
            "descriptorSetBindsPerFrame": submission.get(
                "descriptorSetBindsPerFrame"),
        },
        "renderPath": report.get("renderPath"),
        "gpu": report.get("gpu"),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--stage", required=True,
                        help="Engine-evolution stage label, for example E0.")
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=720)
    parser.add_argument("--frames", type=int, default=300)
    parser.add_argument(
        "--output-dir", type=Path,
        default=PROJECT_ROOT / "captures" / "performance")
    parser.add_argument("--summary", type=Path)
    args = parser.parse_args()

    if not args.executable.is_file():
        print(f"executable not found: {args.executable}", file=sys.stderr)
        return 2

    stage_dir = args.output_dir / args.stage
    results = [
        run_scene(args.executable, scene, args.width, args.height,
                  args.frames, stage_dir)
        for scene in SCENES
    ]

    failures = [r for r in results if r["status"] != "ok"]
    summary = {
        "schemaVersion": 1,
        "stage": args.stage,
        "capturedAtUtc": datetime.now(timezone.utc).isoformat(
            timespec="seconds"),
        "host": {
            "os": platform.platform(),
            "cpu": platform.processor(),
        },
        "settings": {
            "width": args.width,
            "height": args.height,
            "frames": args.frames,
        },
        "scenes": results,
        "failureCount": len(failures),
    }

    encoded = json.dumps(summary, indent=2) + "\n"
    summary_path = args.summary or (stage_dir / "summary.json")
    summary_path.parent.mkdir(parents=True, exist_ok=True)
    summary_path.write_text(encoded, encoding="utf-8")

    for result in results:
        if result["status"] != "ok":
            print(f"[FAIL] {result['scene']}: "
                  f"{result.get('reason') or result.get('exitCode')}")
            continue
        gpu = result["gpuMs"]
        sub = result["submission"]
        print(
            f"[ OK ] {result['scene']:10s} "
            f"total avg={gpu['totalAverage']:.3f}ms "
            f"p95={gpu['totalP95']:.3f}ms p99={gpu['totalP99']:.3f}ms "
            f"draws/frame={sub['drawCallsPerFrame']:.1f} "
            f"binds/frame={sub['descriptorSetBindsPerFrame']:.1f}"
        )
    print(f"stage={args.stage} summary={summary_path} "
          f"failures={len(failures)}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
