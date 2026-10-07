"""Validate unchanged cinematic images and the local 20 ms GPU budget."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import statistics
import subprocess
import time

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def evaluate_budget(values):
    if len(values) != 3 or any(not math.isfinite(v) or v < 0 for v in values):
        raise ValueError("Budget requires exactly three finite nonnegative run averages")
    median = statistics.median(values)
    return {"averagesMs": values, "medianMs": median, "limitExclusiveMs": 20.0,
            "passed": median < 20.0}


def validate_report(report):
    if (report["samples"], report["submission"]["frames"], report["width"], report["height"]) != (300, 300, 1280, 720):
        raise ValueError("Budget requires 300 complete frames at 1280x720")
    for value in (report["totalAverageMs"], report["totalP95Ms"], report["submission"]["recordingMilliseconds"]):
        if not math.isfinite(value) or value < 0:
            raise ValueError("Invalid timing report")


def run(executable, arguments, output, name):
    command = [str(executable), *arguments]
    if executable.parent.name == "bin":
        command += ["--resource-root", str(executable.parent.parent / "share/AzureRender")]
    started = time.monotonic()
    result = subprocess.run(command, cwd=ROOT, capture_output=True, encoding="utf-8", errors="replace", timeout=180)
    elapsed = time.monotonic() - started
    log = result.stdout + result.stderr
    (output / f"{name}.log").write_text(log, encoding="utf-8")
    if result.returncode or "VUID-" in log or "Validation Error" in log:
        raise RuntimeError(f"{name} failed; inspect its log")
    if "Allocator after unload: buffers=0 images=0" not in log:
        raise RuntimeError("GPU resource unload failed")
    return elapsed


def measure(executable, output, repeats):
    runs = []
    for repeat in range(repeats):
        name = f"timing-{repeat}"
        path = output / f"{name}.json"
        seconds = run(executable, ["--scene-type", "blackhole", "--blackhole-quality", "cinematic",
            "--blackhole-camera", "front", "--fixed-frame-step", "--width", "1280", "--height", "720",
            "--smoke-frames", "300", "--gpu-timing", "--gpu-timing-output", str(path)], output, name)
        data = json.loads(path.read_text(encoding="utf-8"))
        validate_report(data)
        runs.append({"report": data, "processSecondsIncludingStartup": seconds})
        print(f"run {repeat}: {data['totalAverageMs']:.6f} ms GPU", flush=True)
    return runs


def compare_images(reference, candidate, output):
    results = {}
    cases = [(camera, camera, 4) for camera in ("front", "orbit-left", "high", "close", "over-shoulder")]
    cases.append(("rotation", "front", 60))
    for case, camera, frames in cases:
        hashes = {}
        reference_pixels = None
        for label, executable in (("reference", reference), ("candidate", candidate)):
            directory = output / f"{case}-{label}"
            run(executable, ["--scene-type", "blackhole", "--blackhole-quality", "cinematic",
                "--blackhole-camera", camera, "--width", "1280", "--height", "720",
                "--capture-dir", str(directory), "--capture-frames", str(frames), "--capture-fps", "60"],
                output, f"{case}-{label}")
            images = sorted(directory.glob("frame_*.png"))
            if len(images) != frames:
                raise ValueError("Incomplete image comparison")
            manifest = json.loads((directory / "capture_manifest.json").read_text(encoding="utf-8"))
            if (manifest["blackhole"]["traceSamplesPerPixel"], manifest["blackhole"]["maxTraceSteps"],
                manifest["blackhole"]["quality"], manifest["width"], manifest["height"]) != (4, 1800, "cinematic", 1280, 720):
                raise ValueError("Cinematic configuration changed")
            pixels = []
            for path in images:
                with Image.open(path) as image:
                    image.load()
                    pixels.append(hashlib.sha256(image.convert("RGBA").tobytes()).hexdigest())
            hashes[label] = pixels
            if reference_pixels is None:
                reference_pixels = pixels
        results[case] = {"frames": frames, "hashes": hashes, "passed": hashes["reference"] == hashes["candidate"]}
        (output / "image-comparison.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
        if not results[case]["passed"]:
            raise ValueError(f"Image pixels differ in {case}")
        print(f"{case}: {frames} frames match", flush=True)
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--baseline-executable", type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--mode", choices=("screen", "performance", "images"), required=True)
    args = parser.parse_args()
    if args.mode == "images" and args.baseline_executable is None:
        parser.error("images requires baseline-executable")
    output = args.output_dir.resolve()
    if output.exists() and any(output.iterdir()):
        parser.error("Use an empty output directory")
    output.mkdir(parents=True, exist_ok=True)
    document = {"status": "running", "mode": args.mode}
    try:
        if args.mode == "images":
            document["images"] = compare_images(args.baseline_executable.resolve(), args.executable.resolve(), output)
        else:
            document["runs"] = measure(args.executable.resolve(), output, 1 if args.mode == "screen" else 3)
            averages = [run["report"]["totalAverageMs"] for run in document["runs"]]
            document["budget"] = (evaluate_budget(averages) if len(averages) == 3 else
                                  {"averageMs": averages[0], "passed": averages[0] < 20.0, "screenOnly": True})
            if not document["budget"]["passed"]:
                raise ValueError("Cinematic GPU budget exceeds 20 ms")
        document["status"] = "passed"
    except Exception as error:
        document.update(status="failed", error=str(error))
        raise
    finally:
        (output / "summary.json").write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
