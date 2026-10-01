"""Exercise audit failures against the actual application with bounded waits."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile


def run(executable, arguments, expected_success=True):
    result = subprocess.run(
        [str(executable), *arguments], capture_output=True, text=True,
        encoding="utf-8", errors="replace", timeout=30)
    output = result.stdout + result.stderr
    if (result.returncode == 0) != expected_success:
        raise RuntimeError(output)
    if "Validation Error" in output or "VUID-" in output:
        raise RuntimeError(output)
    return output


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", required=True, type=Path)
    args = parser.parse_args()
    executable = args.executable.resolve()
    source = Path(__file__).resolve().parents[1]
    sample = run(executable, ["--scene-type", "sample", "--gpu-timing",
                              "--smoke-frames", "30"])
    if "GPU timing samples: 30" not in sample:
        raise RuntimeError("Sample did not provide every requested timing sample")
    asset = source / "assets_public" / "test_model.gltf"
    document = json.loads(asset.read_text(encoding="utf-8"))
    for category in ("buffers", "images"):
        for item in document.get(category, []):
            uri = item.get("uri", "")
            if uri and not uri.startswith("data:"):
                item["uri"] = (asset.parent / uri).resolve().as_posix()
    primitive = document["meshes"][0]["primitives"][0]
    target = primitive["targets"][0]
    primitive["targets"] = [target, target, target]
    with tempfile.TemporaryDirectory(prefix="azr-morph-limit-") as directory:
        fixture = Path(directory) / "too-many-targets.gltf"
        fixture.write_text(json.dumps(document), encoding="utf-8")
        output = run(executable, ["--asset", str(fixture), "--smoke-frames", "1"], False)
        if "supports at most 2" not in output:
            raise RuntimeError("Morph fixture failed without the expected limit diagnostic: " + output)
    print("Sample timing and Morph limit regressions passed")


if __name__ == "__main__":
    main()
