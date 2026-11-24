"""Record the cinematic GPU budget with driver-based environment observations."""
import argparse
import json
import pathlib
import subprocess
import time
from run_blackhole_optimization import evaluate_budget, run, validate_report
from gpu_environment import wait_and_record,snapshot,environment_errors

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
            environment=wait_and_record(output/f'environment-{index}.json')
            temperature=environment['startup'][-1]['temperature']
            path = output / f"timing-{index}.json"
            seconds = run(args.executable.resolve(), [
                "--scene-type", "blackhole", "--blackhole-quality", "cinematic", "--blackhole-camera", "front",
                "--fixed-frame-step", "--width", "1280", "--height", "720", "--smoke-frames", "300",
                "--gpu-timing", "--gpu-timing-output", str(path)
            ], output, f"timing-{index}")
            report = json.loads(path.read_text())
            validate_report(report)
            environment['final']=snapshot()
            (output/f'environment-{index}.json').write_text(json.dumps(environment,indent=2)+'\n',encoding='utf-8')
            records.append({"startTemperatureC": temperature, "environment":environment,"report": report, "processSecondsIncludingStartup": seconds})
            print(f"round {index}: {report['totalAverageMs']:.6f} ms, start {temperature} C", flush=True)
    finally:
        monitor.terminate()
        monitor.wait()
        budget = evaluate_budget([entry["report"]["totalAverageMs"] for entry in records]) if len(records) == 3 else None
        (output / "summary.json").write_text(json.dumps({
            "environmentPolicy": "DriverLimitsAndStableIdle", "runs": records, "budget": budget,
            "status": "passed" if budget and budget["passed"] and all(not environment_errors(r['environment']) for r in records) else "failed"
        }, indent=2) + "\n")
if not budget["passed"] or any(environment_errors(r['environment']) for r in records):
    raise RuntimeError("Cinematic GPU budget failed")
