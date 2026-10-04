"""Download hash-pinned MIT ufbx and build the optional host-only sampler."""
import argparse
import hashlib
from pathlib import Path
import subprocess
import urllib.request

FILES = {"ufbx.h": "d2e3a7038f81fe8742705bff96e9c072222bd14bef85a97fa778fa858835d07e",
    "ufbx.c": "a6663d3be07e68b6b4b4fbca692e21a7449097f60b83fdc2b634f00b16d50315"}
parser = argparse.ArgumentParser(__doc__)
parser.add_argument("--output-dir", type=Path, default=Path("build/f1"))
args = parser.parse_args()
root = args.output_dir.resolve()
vendor = root / "ufbx"
vendor.mkdir(parents=True, exist_ok=True)
for name, expected in FILES.items():
    path = vendor / name
    if not path.is_file():
        urllib.request.urlretrieve("https://raw.githubusercontent.com/ufbx/ufbx/v0.17.1/" + name, path)
    if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
        raise ValueError("ufbx source hash differs: " + name)
license_path = vendor / "LICENSE"
if not license_path.is_file():
    urllib.request.urlretrieve("https://raw.githubusercontent.com/ufbx/ufbx/v0.17.1/LICENSE", license_path)
source = Path(__file__).with_name("fbx_sample.c").resolve()
subprocess.run(["cl", "/nologo", "/O2", "/TC", "/I" + str(vendor), str(source), str(vendor / "ufbx.c"),
    "/Fe:" + str(root / "azure_fbx_sample.exe"), "/Fo:" + str(root) + "\\"], cwd=root, check=True)
