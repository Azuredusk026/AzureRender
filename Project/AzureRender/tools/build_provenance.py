"""Build and associate runtime inputs with products, then reject stale results."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

INPUT_DIRS = ("src", "shaders", "tools", "schemas", "assets_public", "cmake", "managed", "third_party/dotnet")
INPUT_FILES = ("CMakeLists.txt", "CMakePresets.json", "vcpkg.json", "vcpkg-configuration.json")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def product_paths(build_dir, configuration):
    prefix = build_dir / configuration if (build_dir / configuration).is_dir() else build_dir
    suffix = ".exe" if __import__("os").name == "nt" else ""
    products=[prefix / (name + suffix) for name in
        ("AzureRender", "AzurePlayer", "AzureMetaGen", "AzureGeometryCompiler")]
    cache=build_dir/'CMakeCache.txt'
    if cache.is_file() and 'AZURE_ENABLE_MANAGED_SCRIPT_PROTOTYPE:BOOL=ON' in cache.read_text(encoding='utf-8'):
        products += [build_dir/'managed'/name for name in (
            'coreclr/Azure.Engine.dll','coreclr/Azure.Engine.runtimeconfig.json','coreclr/Azure.Engine.deps.json',
            'coreclr/Azure.Samples.dll','nativeaot/Azure.Engine.Native.dll','manifest.json','complete.stamp')]
    return products


def inputs(source):
    paths = [source / name for name in INPUT_FILES if (source / name).is_file()]
    for name in INPUT_DIRS:
        paths.extend(p for p in (source / name).rglob("*") if p.is_file()
            and not any(part in {"__pycache__", ".azure"} for part in p.parts))
    return {p.relative_to(source).as_posix(): digest(p) for p in sorted(paths)}


def describe(source, products):
    commit = subprocess.run(["git", "-C", str(source), "rev-parse", "HEAD"],
        capture_output=True, text=True, encoding="utf-8", errors="replace").stdout.strip()
    status = subprocess.run(["git", "-C", str(source), "status", "--porcelain"],
        capture_output=True, text=True, encoding="utf-8", errors="replace").stdout.strip()
    return {"schemaVersion": 1, "commit": commit, "workingTreeDirty": bool(status),
        "source": inputs(source), "products": {str(p.resolve()): digest(p) for p in products}}


def verify(source, manifest):
    if inputs(source) != manifest["source"]:
        raise ValueError("Build source differs from recorded inputs")
    for name, expected in manifest["products"].items():
        if not Path(name).is_file() or digest(Path(name)) != expected:
            raise ValueError("Build product differs: " + name)


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("mode", choices=["build", "verify"])
    parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--config", choices=["Debug", "Release"], default="Release")
    args = parser.parse_args()
    manifest_path = args.build_dir / "build-provenance.json"
    if args.mode == "build":
        before = inputs(args.source)
        subprocess.run(["cmake", "--build", str(args.build_dir), "--config", args.config], check=True)
        manifest = describe(args.source, product_paths(args.build_dir, args.config))
        if before != manifest["source"]:
            raise ValueError("Build source changed during compilation")
        manifest["configuration"] = args.config
        manifest["toolchain"] = (args.build_dir / "CMakeCache.txt").read_text(encoding="utf-8")
        manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    verify(args.source, json.loads(manifest_path.read_text(encoding="utf-8")))
    print(json.dumps({"status": "passed", "manifest": str(manifest_path)}))


if __name__ == "__main__":
    main()
