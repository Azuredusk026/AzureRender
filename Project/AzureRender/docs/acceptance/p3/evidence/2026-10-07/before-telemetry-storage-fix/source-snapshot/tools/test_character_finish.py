"""Capture and measure character appearance on the real Vulkan backend."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import numpy as np
from PIL import Image

CAMERAS = ["full-body-front", "face-front", "face-three-quarter-left",
           "face-three-quarter-right", "face-side-left", "face-side-right", "back-detail"]
LIGHTS = ["neutral-material", "stylized-key", "specular-rim"]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def pixels(path):
    return np.asarray(Image.open(path).convert("RGB"), dtype=np.int16)


def capture(executable, asset, root, name, camera, light, isolation="beauty", frames=1, extra=()):
    output = root / name
    if output.exists():
        raise FileExistsError(output)
    command = [str(executable), "--asset", str(asset), "--width", "1280", "--height", "720",
               "--qa-camera", camera, "--qa-light", light, "--qa-isolation", isolation,
               "--capture-dir", str(output), "--capture-frames", str(frames), "--capture-fps", "60",
               "--fixed-frame-step", *extra]
    startup = None
    if hasattr(subprocess, "STARTUPINFO"):
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
    result = subprocess.run(command, cwd=executable.parent, capture_output=True, text=True,
                            encoding="utf-8", errors="replace", timeout=240, startupinfo=startup)
    log = result.stdout + result.stderr
    (root / (name + ".log")).write_text(log, encoding="utf-8")
    if result.returncode or "VUID-" in log or "Validation Error" in log:
        raise RuntimeError(f"GPU capture failed: {name}, exit={result.returncode}; see log")
    if "Allocator after unload: buffers=0 images=0" not in log:
        raise RuntimeError(f"Resource cleanup missing: {name}")
    manifest = json.loads((output / "capture_manifest.json").read_text())
    if manifest["capturedFrames"] != frames:
        raise RuntimeError(f"Incomplete capture: {name}")
    return output


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--asset", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--camera", choices=CAMERAS, action="append")
    parser.add_argument("--light", choices=LIGHTS, action="append")
    parser.add_argument("--annotations", type=Path, help="Frozen JSON mapping camera to brow-mask PNG")
    parser.add_argument("--light-scans", action="store_true")
    parser.add_argument("--scan-only", action="store_true")
    args = parser.parse_args()
    executable, asset, root = args.executable.resolve(), args.asset.resolve(), args.output.resolve()
    if root.exists() and any(root.iterdir()):
        raise ValueError("Output directory must be empty")
    root.mkdir(parents=True, exist_ok=True)
    annotations = json.loads(args.annotations.read_text()) if args.annotations else {}
    report = {"version": 1, "assetSha256": digest(asset), "executableSha256": digest(executable),
              "shaderSha256": {p.name: digest(p) for p in sorted((executable.parent / "shaders").glob("*.spv"))},
              "windowMode": "hidden", "cases": [], "failures": []}
    frozen_inputs = executable.parent / "qa-inputs.json"
    if frozen_inputs.is_file():
        report["frozenRuntimeManifestSha256"] = digest(frozen_inputs)
    def check(condition, message):
        if not condition:
            report["failures"].append(message)
    for camera in ([] if args.scan_only else args.camera or CAMERAS):
        for light in args.light or LIGHTS:
            prefix = camera + "_" + light
            beauty = capture(executable, asset, root, prefix + "_beauty", camera, light, frames=3)
            mask_dir = capture(executable, asset, root, prefix + "_mask", camera, light, "brow-mask")
            disabled = capture(executable, asset, root, prefix + "_off", camera, light,
                               extra=("--qa-effect", "overlay", "--qa-effect-state", "disabled"))
            cpu = capture(executable, asset, root, prefix + "_cpu", camera, light,
                          extra=("--disable-compute-skinning",))
            first = pixels(beauty / "frame_000000.png")
            mask = np.min(pixels(mask_dir / "frame_000000.png"), axis=2) > 200
            difference = np.max(np.abs(first - pixels(disabled / "frame_000000.png")), axis=2)
            visible = int(mask.sum())
            contribution = float(np.mean(difference[mask] > 1)) if visible else 0
            cpu_delta = np.abs(first - pixels(cpu / "frame_000000.png"))
            stationary = all(np.array_equal(first, pixels(p)) for p in sorted(beauty.glob("frame_*.png")))
            case = {"camera": camera, "light": light, "visibleBrowPixels": visible,
                    "browContribution": contribution, "stationaryExact": stationary,
                    "cpuComputeMeanDifference": float(cpu_delta.mean()),
                    "cpuComputePixelsOver3": int(np.count_nonzero(cpu_delta.max(axis=2) > 3))}
            if camera.startswith("face-"):
                check(visible > 0, prefix + ": no visible eyebrow")
                check(contribution >= .9, prefix + ": brow contribution below 90%")
                if camera in annotations:
                    reference_path = (args.annotations.parent / annotations[camera]).resolve()
                    reference = np.min(pixels(reference_path), axis=2) > 200
                    if not reference.any():
                        raise ValueError("Empty frozen annotation: " + camera)
                    coverage = float(np.mean(mask[reference] & (difference[reference] > 1)))
                    case["annotationSha256"] = digest(reference_path)
                    case["annotationCoverage"] = coverage
                    check(coverage >= .9, prefix + ": frozen annotation coverage below 90%")
            check(stationary, prefix + ": stationary frame changed")
            check(cpu_delta.mean() <= .05 and case["cpuComputePixelsOver3"] <= 20,
                  prefix + ": CPU/Compute image mismatch")
            report["cases"].append(case)
            (root / "summary.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
            print(json.dumps(case), flush=True)
    if args.light_scans:
        for camera in ("face-front", "face-three-quarter-left"):
            output = capture(executable, asset, root, camera + "_light_scan", camera, "stylized-key",
                             frames=120, extra=("--qa-light-scan",))
            # Face annotations specify a fixed, reviewed region excluding real cast-shadow boundaries.
            specification = annotations.get(camera + "_face")
            if not specification:
                raise ValueError("A frozen face region is required for " + camera)
            region = np.asarray(Image.open(args.annotations.parent / specification).convert("L")) > 200
            if not region.any():
                raise ValueError("Empty face annotation")
            means = [float(pixels(p)[region].mean()) for p in sorted(output.glob("frame_*.png"))]
            luminances = [float((pixels(p)[region] @ np.array([.2126, .7152, .0722])).mean())
                          for p in sorted(output.glob("frame_*.png"))]
            delta = float(np.max(np.abs(np.diff(means))))
            luminance_delta = float(np.max(np.abs(np.diff(luminances))))
            report.setdefault("lightScans", []).append({"camera": camera, "frames": len(means),
                "maximumAdjacentMeanDifference255": delta, "maximumAdjacentLuminanceDifference255": luminance_delta,
                "faceRegionSha256": digest(args.annotations.parent / specification)})
            check(max(delta, luminance_delta) <= 3, camera + ": face light discontinuity above 3/255")
    report["passed"] = not report["failures"]
    if digest(asset) != report["assetSha256"] or digest(executable) != report["executableSha256"]:
        raise RuntimeError("Capture input changed during the run")
    (root / "summary.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    if report["failures"]:
        raise SystemExit("\n".join(report["failures"]))


if __name__ == "__main__":
    main()
