"""Validate offline FBX sampling against a supplied real clip."""
import argparse
import json
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--executable", type=Path, required=True)
parser.add_argument("--idle", type=Path, required=True)
args = parser.parse_args()
assert args.executable.is_file(), "Offline sampler is missing"
bad = subprocess.run([str(args.executable.resolve()), "missing.fbx"], capture_output=True)
assert bad.returncode != 0 and b"load" in bad.stderr.lower(), bad
good = subprocess.run([str(args.executable.resolve()), str(args.idle.resolve())], capture_output=True)
assert good.returncode == 0, good.stderr
data = json.loads(good.stdout)
assert len([n for n in data["nodes"] if n["bone"]]) == 65
assert data["clips"][0]["duration"] > 8
assert len(data["clips"][0]["frames"]) == 251
assert any(n["name"] == "mixamorig:Hips" for n in data["nodes"])
print("FBX invalid input and real idle sampling passed")
