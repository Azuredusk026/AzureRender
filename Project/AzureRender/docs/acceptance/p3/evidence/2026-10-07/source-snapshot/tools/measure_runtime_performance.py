"""Record the existing cinematic GPU budget with the G0 starting-temperature rule."""
import argparse
import json
import pathlib
import subprocess
import time
from run_blackhole_optimization import evaluate_budget, run, validate_report

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--executable", required=True, type=pathlib.Path)
parser.add_argument("--output-dir", required=True, type=pathlib.Path)
args = parser.parse_args()
output = args.output_dir.resolve()
output.mkdir(parents=True, exist_ok=True)
if any(output.iterdir()):
    parser.error("Use an empty output directory")
records = []
with (output / "gpu-state.csv").open("w") as log:
    monitor = subprocess.Popen([
        "nvidia-smi", "--query-gpu=timestamp,temperature.gpu,pstate,clocks.current.graphics,power.draw,utilization.gpu",
        "--format=csv", "-l", "1"
    ], stdout=log, stderr=subprocess.STDOUT)
    try:
        for index in range(3):
            start = time.monotonic()
            while True:
                temperature = int(subprocess.check_output([
                    "nvidia-smi", "--query-gpu=temperature.gpu", "--format=csv,noheader,nounits"
                ], text=True).strip())
                if temperature <= 55:
                    break
                if time.monotonic() - start > 180:
                    raise RuntimeError("GPU did not reach the G0 starting-temperature condition")
                time.sleep(5)
            path = output / f"timing-{index}.json"
            seconds = run(args.executable.resolve(), [
                "--scene-type", "blackhole", "--blackhole-quality", "cinematic", "--blackhole-camera", "front",
                "--fixed-frame-step", "--width", "1280", "--height", "720", "--smoke-frames", "300",
                "--gpu-timing", "--gpu-timing-output", str(path)
            ], output, f"timing-{index}")
            report = json.loads(path.read_text())
            validate_report(report)
            records.append({"startTemperatureC": temperature, "report": report, "processSecondsIncludingStartup": seconds})
            print(f"round {index}: {report['totalAverageMs']:.6f} ms, start {temperature} C", flush=True)
    finally:
        monitor.terminate()
        monitor.wait()
        budget = evaluate_budget([entry["report"]["totalAverageMs"] for entry in records]) if len(records) == 3 else None
        (output / "summary.json").write_text(json.dumps({
            "thermalStartMaximumC": 55, "runs": records, "budget": budget,
            "status": "passed" if budget and budget["passed"] else "failed"
        }, indent=2) + "\n")
if not budget["passed"]:
    raise RuntimeError("Cinematic GPU budget failed")
