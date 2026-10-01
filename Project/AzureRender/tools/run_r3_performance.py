"""Run sequential fixed-input R3 performance comparisons."""
import argparse
import json
from pathlib import Path
import statistics
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--instances", type=int, default=256)
    parser.add_argument("--frames", type=int, default=300)
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument("--scene", type=Path)
    parser.add_argument("--generate-resources", type=int, default=0)
    parser.add_argument("--check-recording-budget", action="store_true")
    args = parser.parse_args()
    if min(args.instances, args.frames, args.repeats) < 1:
        parser.error("instances, frames and repeats must be positive")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="run-", dir=args.output_dir.resolve()))
    if args.generate_resources:
        if args.scene or args.generate_resources < 1:
            parser.error("generate-resources must be positive and cannot accompany scene")
        asset = Path(__file__).resolve().parents[1] / "assets_public" / "test_model.gltf"
        scene = output / "multi-resource.azscene"
        subprocess.run([str(args.executable.resolve()), "--asset", str(asset),
                        "--create-scene", str(scene)], check=True, capture_output=True, timeout=30)
        text = scene.read_text(encoding="utf-8")
        start, end = text.index("resourceCount "), text.index("lightCount ")
        count = args.generate_resources
        lines = [f"resourceCount {count}"]
        lines += [f'resource "asset-{i}" "gltf" "{asset.as_posix()}"' for i in range(count)]
        lines.append(f"nodeCount {count}")
        for i in range(count):
            lines += [f'node "node-{i}" "mesh-{i}" "" "asset-{i}"', "visible true",
                      f"transform {(i % 8) * 0.15} 0 {(i // 8) * 0.15} 0 0 0 1 1 1", 'prefab "" ""']
        scene.write_text(text[:start] + "\n".join(lines) + "\n" + text[end:], encoding="utf-8")
        args.scene = scene
        args.instances = count
    modes = {
        "parallel_gpu": [], "serial_gpu": ["--disable-parallel-recording"],
        "parallel_cpu": ["--disable-gpu-culling"],
        "serial_cpu": ["--disable-gpu-culling", "--disable-parallel-recording"],
    }
    results = {name: [] for name in modes}
    for repeat in range(args.repeats):
        mode_order = list(modes)
        if repeat % 2:
            mode_order.reverse()
        for name in mode_order:
            switches = modes[name]
            report = output / f"{name}-{repeat}.json"
            command = [str(args.executable.resolve()), "--fixed-frame-step",
                "--qa-camera", "full-body-front", "--instances", str(args.instances),
                "--width", "1280", "--height", "720", "--smoke-frames", str(args.frames),
                "--gpu-timing", "--gpu-timing-output", str(report), *switches]
            if args.scene:
                command.extend(["--scene", str(args.scene.resolve())])
            run = subprocess.run(command, capture_output=True, text=True,
                                 encoding="utf-8", errors="replace", timeout=180)
            log = run.stdout + run.stderr
            (output / f"{name}-{repeat}.log").write_text(log, encoding="utf-8")
            if run.returncode or "VUID-" in log or "Validation Error" in log:
                raise RuntimeError(f"Performance run failed: {name}-{repeat}")
            data = json.loads(report.read_text(encoding="utf-8"))
            submission = data["submission"]
            if data["samples"] != args.frames or submission["frames"] != args.frames:
                raise RuntimeError("Incomplete performance sample")
            results[name].append({"cpuRecordingMs": submission["recordingMilliseconds"] / args.frames,
                "gpuMs": data["totalAverageMs"], "draws": submission["drawCallsPerFrame"],
                "workerPasses": submission["workerRecordedPasses"]})
            results[name][-1]["visibleRatio"] = submission["visibleRatio"]
            results[name][-1]["indirectDrawCalls"] = submission["indirectDrawCalls"]
            results[name][-1]["workerChunks"] = submission["workerRecordedChunks"]
            for key in ("graphPreparationMilliseconds", "workerWaitMilliseconds", "graphExecutionMilliseconds"):
                results[name][-1][key] = submission[key] / args.frames
            if name.startswith("serial") and submission["workerRecordedPasses"] != 0:
                raise RuntimeError("Serial performance path used recording workers")
            if name.startswith("parallel") and submission["workerRecordedPasses"] == 0:
                raise RuntimeError("Parallel performance path did not use recording workers")
            if submission["instances"] != args.instances * args.frames:
                raise RuntimeError("Unexpected instance count in performance sample")
    ratios = [row["visibleRatio"] for rows in results.values() for row in rows]
    if max(ratios) - min(ratios) > 0.000001:
        raise RuntimeError("Fixed-input runs produced different reference visibility ratios")
    summary = {name: {key: statistics.median(row[key] for row in rows)
                     for key in rows[0]} for name, rows in results.items()}
    document = {"instances": args.instances, "frames": args.frames, "repeats": args.repeats,
                "scene": str(args.scene.resolve()) if args.scene else None,
                "samples": results, "median": summary}
    document["spread"] = {name: {
        "cpuMinimumMs": min(row["cpuRecordingMs"] for row in rows),
        "cpuMaximumMs": max(row["cpuRecordingMs"] for row in rows),
        "gpuMinimumMs": min(row["gpuMs"] for row in rows),
        "gpuMaximumMs": max(row["gpuMs"] for row in rows)} for name, rows in results.items()}
    document["pairedRatios"] = [{
        "repeat": i,
        "cpuRatio": results["parallel_gpu"][i]["cpuRecordingMs"] / results["serial_gpu"][i]["cpuRecordingMs"],
        "gpuRatio": results["parallel_gpu"][i]["gpuMs"] / results["serial_gpu"][i]["gpuMs"]}
        for i in range(args.repeats)]
    if args.check_recording_budget:
        if args.generate_resources != 32 or args.frames != 300 or args.repeats < 3:
            raise RuntimeError("Recording budget requires 32 generated resources, 300 frames and at least three repeats")
        cpu_ratio = summary["parallel_gpu"]["cpuRecordingMs"] / summary["serial_gpu"]["cpuRecordingMs"]
        gpu_ratio = summary["parallel_gpu"]["gpuMs"] / summary["serial_gpu"]["gpuMs"]
        document["budget"] = {"cpuRatio": cpu_ratio, "gpuRatio": gpu_ratio,
            "passed": cpu_ratio <= 0.95 and gpu_ratio <= 1.05}
    (output / "summary.json").write_text(json.dumps(document, indent=2), encoding="utf-8")
    print(json.dumps({"output": str(output), "median": summary}, indent=2))
    if args.check_recording_budget and not document["budget"]["passed"]:
        raise RuntimeError("R3 recording performance budget failed; inspect summary.json")


if __name__ == "__main__":
    main()
