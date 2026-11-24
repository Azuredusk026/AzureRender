"""Reproduce the compiled-control red and arithmetic-order green on a fixed capture."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--engine", type=Path, required=True)
parser.add_argument("--renderdoc", type=Path, required=True)
parser.add_argument("--capture", type=Path, required=True)
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
engine = args.engine.resolve()
capture = args.capture.resolve()
output = args.output.resolve()
here = Path(__file__).resolve().parent
sys.path.insert(0, str(engine / "tools"))
from native_render.shader_arithmetic_order import normalize_shader

sha = lambda data: hashlib.sha256(data).hexdigest()
reference = json.loads((here / "reference.json").read_text(encoding="utf-8-sig"))
with capture.open("rb") as source:
    digest = hashlib.file_digest(source, "sha256").hexdigest()
assert digest == reference["captureSha256"] and capture.stat().st_size == reference["captureBytes"]
output.mkdir()
fixtures = engine / "tests/fixtures/shader_arithmetic_order"
compiled = (fixtures / "compiled-ray-mirror.dxbc").read_bytes()
policy_data = (fixtures / "ray-policy.json").read_bytes()
policy = json.loads(policy_data.decode("utf-8-sig"))
normalized = normalize_shader(compiled, policy)
variants = []
for label, data in (("compiled-control", compiled), ("normalized-tool", normalized)):
    path = output / (label + ".dxbc")
    path.write_bytes(data)
    variants.append(dict(label=label, path=str(path), sha256=sha(data)))
request = dict(capture=str(capture), output=str(output), reference=reference, variants=variants)
request_path = output / "request.json"
request_path.write_text(json.dumps(request, indent=2), encoding="utf-8")
environment = os.environ.copy()
removed = [key for key in ("VK_LAYER_PATH", "VK_INSTANCE_LAYERS", "ENABLE_VULKAN_RENDERDOC_CAPTURE")
           if key in environment]
for key in removed:
    environment.pop(key)
environment["AZURE_ORDER_REPLAY_REQUEST"] = str(request_path)
worker = here / "replay_worker.py"
command = [str(args.renderdoc.resolve()), "--python", str(worker)]
startup = subprocess.STARTUPINFO()
startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
startup.wShowWindow = 0
receipt = dict(startUtc=datetime.datetime.now(datetime.timezone.utc).isoformat(), command=command,
               captureSha256=digest, policySha256=sha(policy_data), workerSha256=sha(worker.read_bytes()),
               normalizerSha256=sha((engine / "tools/native_render/shader_arithmetic_order.py").read_bytes()),
               renderdocSha256=sha(args.renderdoc.read_bytes()), timeoutSeconds=300,
               removedCaptureEnvironmentKeys=removed)
with (output / "worker.log").open("w", encoding="utf-8") as log:
    process = subprocess.Popen(command, cwd=output, env=environment, startupinfo=startup,
                               stdout=log, stderr=subprocess.STDOUT)
    receipt["pid"] = process.pid
    (output / "receipt.json").write_text(json.dumps(receipt, indent=2), encoding="utf-8")
    try:
        result = process.wait(timeout=300)
    except subprocess.TimeoutExpired:
        subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"], stdout=log, stderr=subprocess.STDOUT)
        process.wait()
        raise
receipt.update(exitCode=result, endUtc=datetime.datetime.now(datetime.timezone.utc).isoformat())
(output / "receipt.json").write_text(json.dumps(receipt, indent=2), encoding="utf-8")
report = json.loads((output / "result.json").read_text(encoding="utf-8-sig"))
assert result == 0 and report["status"] == "passed", report
print(json.dumps(dict(status=report["status"], variants=[dict(label=row["label"],
    differentBytes=[value["differentBytes"] for value in row["outputs"]]) for row in report["variants"]]), indent=2))
