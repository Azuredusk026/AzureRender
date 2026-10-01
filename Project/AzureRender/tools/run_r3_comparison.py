"""Deterministic R3 recording and visibility path comparison."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--frames", type=int, default=30)
    parser.add_argument("--instances", type=int, default=256)
    parser.add_argument("--transparent-fixture", action="store_true")
    args = parser.parse_args()
    if args.frames < 1 or args.instances < 1:
        parser.error("frames and instances must be positive")
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="run-", dir=output))
    asset_arguments = []
    if args.transparent_fixture:
        asset = Path(__file__).resolve().parents[1] / "assets_public" / "test_model.gltf"
        document = json.loads(asset.read_text(encoding="utf-8"))
        for category in ("buffers", "images"):
            for item in document.get(category, []):
                uri = item.get("uri", "")
                if uri and not uri.startswith("data:"):
                    item["uri"] = (asset.parent / uri).resolve().as_posix()
        for material in document.get("materials", []):
            material["alphaMode"] = "BLEND"
            material.setdefault("pbrMetallicRoughness", {})["baseColorFactor"] = [1, 1, 1, 0.5]
        fixture = output / "transparent.gltf"
        fixture.write_text(json.dumps(document), encoding="utf-8")
        asset_arguments = ["--asset", str(fixture)]
    modes = {
        "parallel_gpu": [],
        "serial_gpu": ["--disable-parallel-recording"],
        "parallel_cpu": ["--disable-gpu-culling"],
        "serial_cpu": ["--disable-parallel-recording", "--disable-gpu-culling"],
        "parallel_single_gpu": ["--disable-multi-draw-indirect"],
    }
    results = {}
    reference = None
    for name, switches in modes.items():
        capture = output / name
        if capture.exists():
            raise RuntimeError(f"Use a fresh output directory; capture exists: {capture}")
        timing = output / f"{name}.json"
        command = [str(args.executable.resolve()), "--instances", str(args.instances),
                   "--width", "1280", "--height", "720", "--qa-camera", "full-body-front",
                   "--capture-dir", str(capture), "--capture-frames", str(args.frames),
                   "--capture-fps", "60", "--gpu-timing", "--gpu-timing-output", str(timing),
                   *asset_arguments, *switches]
        run = subprocess.run(command, capture_output=True, text=True, encoding="utf-8",
                             errors="replace", timeout=180)
        log = run.stdout + run.stderr
        (output / f"{name}.log").write_text(log, encoding="utf-8")
        if run.returncode or "VUID-" in log or "Validation Error" in log:
            raise RuntimeError(f"R3 mode failed: {name}; inspect its log")
        frames = sorted(capture.glob("frame_*.png"))
        if len(frames) != args.frames:
            raise RuntimeError(f"Incomplete capture for {name}: {len(frames)}")
        hashes = [hashlib.sha256(frame.read_bytes()).hexdigest() for frame in frames]
        if reference is None:
            reference = hashes
        report = json.loads(timing.read_text(encoding="utf-8"))
        worker_passes = report["submission"]["workerRecordedPasses"]
        indirect_calls = report["submission"]["indirectDrawCalls"]
        if name.endswith("cpu") and indirect_calls != 0:
            raise RuntimeError(f"CPU mode used indirect drawing: {name}")
        if name.endswith("gpu") and not args.transparent_fixture and indirect_calls == 0:
            raise RuntimeError(f"GPU mode did not consume indirect commands: {name}")
        if name.startswith("serial") and worker_passes != 0:
            raise RuntimeError(f"Serial mode recorded worker passes: {name}")
        if name.startswith("parallel") and worker_passes == 0:
            raise RuntimeError(f"Parallel mode did not record worker passes: {name}")
        results[name] = {"hashes": hashes, "matchesReference": hashes == reference,
                         "timing": report}
    passed = all(result["matchesReference"] for result in results.values())
    (output / "comparison.json").write_text(json.dumps({
        "status": "passed" if passed else "failed", "frames": args.frames,
        "instances": args.instances, "modes": results}, indent=2), encoding="utf-8")
    if not passed:
        raise RuntimeError("R3 paths produced different deterministic captures")
    print("R3 four-path deterministic comparison passed")


if __name__ == "__main__":
    main()
