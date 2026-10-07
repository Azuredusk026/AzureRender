"""Copy a built runtime and shaders into a new immutable QA input directory."""
import argparse
import json
from pathlib import Path
import shutil
from build_provenance import describe


def freeze(executable, output):
    executable, output = executable.resolve(), output.resolve()
    if output.exists():
        raise FileExistsError(output)
    if not (executable.parent / "shaders").is_dir():
        raise ValueError("Compiled shader directory missing")
    output.mkdir(parents=True)
    shutil.copy2(executable, output / executable.name)
    for library in executable.parent.glob("*.dll"):
        shutil.copy2(library, output / library.name)
    shutil.copytree(executable.parent / "shaders", output / "shaders")
    source = Path(__file__).resolve().parents[1]
    products = [p for p in sorted(output.rglob("*")) if p.is_file()]
    manifest = describe(source, products)
    manifest["originExecutable"] = str(executable)
    manifest["originCmakeCacheSha256"] = __import__("hashlib").sha256((executable.parent / "CMakeCache.txt").read_bytes()).hexdigest()
    (output / "qa-inputs.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print(output / executable.name)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    freeze(args.executable, args.output)
