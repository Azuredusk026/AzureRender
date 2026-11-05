"""Build and associate runtime inputs with products, then reject stale results."""
import argparse
import hashlib
import json
import re
from pathlib import Path
import subprocess

INPUT_DIRS = ("src", "shaders", "tools", "schemas", "assets_public", "cmake", "managed", "third_party/dotnet")
INPUT_FILES = ("CMakeLists.txt", "CMakePresets.json", "vcpkg.json", "vcpkg-configuration.json",
               "docs/plans/engine-evolution-manifest.json")


def digest(path):
    value=hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda:stream.read(1024*1024),b""):
            value.update(block)
    return value.hexdigest()


def executable_target(value):
    if not isinstance(value,str) or re.fullmatch(r"[A-Za-z_][A-Za-z0-9_.+-]*",value) is None:
        raise ValueError("Expected a single executable target name")
    return value


def runtime_executable_targets():
    targets=["AzureRender","AzurePlayer","AzureMetaGen","AzureGeometryCompiler",
             "AzureAssetExchange","AzureMaterialValidator"]
    if __import__("os").name=="nt":targets.append("AzureGameLauncher")
    return targets


def scoped_build_targets(build_dir,test_targets):
    targets=[*runtime_executable_targets(),"AzureEngineContracts"]
    cache=build_dir/"CMakeCache.txt"
    text=cache.read_text(encoding="utf-8") if cache.is_file() else ""
    if 'AZURE_ENABLE_MANAGED_SCRIPT_PROTOTYPE:BOOL=ON' in text:
        targets.append("AzureManagedArtifacts")
    if 'AZURE_ENABLE_SHADER_MODULE_PROTOTYPE:BOOL=ON' in text:
        targets.append("AzureShaderModulePrototypes")
    targets.extend(executable_target(name) for name in test_targets)
    return list(dict.fromkeys(targets))


def executable_product_path(build_dir,configuration,target):
    prefix=build_dir/configuration if (build_dir/configuration).is_dir() else build_dir
    suffix=".exe" if __import__("os").name=="nt" else ""
    return prefix/(executable_target(target)+suffix)


def product_paths(build_dir, configuration, test_targets=()):
    prefix = build_dir / configuration if (build_dir / configuration).is_dir() else build_dir
    suffix = ".exe" if __import__("os").name == "nt" else ""
    products=[prefix / (name + suffix) for name in
        ("AzureRender", "AzurePlayer", "AzureMetaGen", "AzureGeometryCompiler")]
    for name in ("AzureAssetExchange","AzureMaterialValidator","AzureGameLauncher"):
        product=prefix/(name+suffix)
        if product.is_file():products.append(product)
    products.extend(sorted((build_dir/"shaders").glob("*.spv")))
    cache=build_dir/'CMakeCache.txt'
    if cache.is_file() and 'AZURE_ENABLE_MANAGED_SCRIPT_PROTOTYPE:BOOL=ON' in cache.read_text(encoding='utf-8'):
        products += [build_dir/'managed'/name for name in (
            'coreclr/Azure.Engine.dll','coreclr/Azure.Engine.runtimeconfig.json','coreclr/Azure.Engine.deps.json',
            'coreclr/Azure.Samples.dll','nativeaot/Azure.Engine.Native.dll','manifest.json','complete.stamp')]
    if cache.is_file() and 'AZURE_ENABLE_SHADER_MODULE_PROTOTYPE:BOOL=ON' in cache.read_text(encoding='utf-8'):
        products += [build_dir/'shader-modules'/name for name in
            ('bloom-direct.spv','bloom-direct.spv.json','bloom-cached.spv','bloom-cached.spv.json')]
    contracts=build_dir/'contracts'
    if (contracts/'manifest.json').is_file():
        from write_engine_contracts import verify_bundle
        snapshot=verify_bundle(contracts)
        products += [contracts/'manifest.json']+[contracts/name for name in snapshot['files']]
    products.extend(executable_product_path(build_dir,configuration,name) for name in test_targets)
    return list(dict.fromkeys(products))


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
    scope=manifest.get("buildScope",{})
    if scope.get("kind")=="runtime-and-tests":
        selected=scope.get("testProducts")
        if not isinstance(selected,dict) or not selected:
            raise ValueError("Scoped test product inventory is missing")
        for target,path in selected.items():
            executable_target(target)
            if (target not in scope.get("targets",[]) or path not in manifest["products"]
                or Path(path).name not in (target,target+".exe")):
                raise ValueError("Scoped test product is absent or mismatched: "+target)
    if inputs(source) != manifest["source"]:
        raise ValueError("Build source differs from recorded inputs")
    for name, expected in manifest["products"].items():
        if not Path(name).is_file() or digest(Path(name)) != expected:
            raise ValueError("Build product differs: " + name)
        path=Path(name)
        if path.name=='manifest.json' and path.parent.name=='contracts':
            from write_engine_contracts import verify_bundle
            verify_bundle(path.parent)


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("mode", choices=["build", "verify"])
    parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--config", choices=["Debug", "Release"], default="Release")
    parser.add_argument("--runtime-only",action="store_true",
        help="Build runtime and contract products without unrelated test targets")
    parser.add_argument("--test-target",action="append",type=executable_target,default=[],
        help="Build and freeze this executable test target with all required runtime products")
    args = parser.parse_args()
    if args.mode != "build" and (args.runtime_only or args.test_target):
        parser.error("Build selection options require build mode")
    test_targets=list(dict.fromkeys(args.test_target))
    manifest_path = args.build_dir / "build-provenance.json"
    if args.mode == "build":
        before = inputs(args.source)
        command=["cmake","--build",str(args.build_dir),"--config",args.config]
        selected=scoped_build_targets(args.build_dir,test_targets) if args.runtime_only or test_targets else []
        if selected:command.extend(["--target",*selected])
        subprocess.run(command,check=True)
        products=product_paths(args.build_dir,args.config,test_targets)
        if selected:
            products.extend(executable_product_path(args.build_dir,args.config,name) for name in runtime_executable_targets())
        manifest=describe(args.source,list(dict.fromkeys(products)))
        manifest["buildScope"]={"kind":"runtime-and-tests" if test_targets else "runtime" if selected else "all",
                                "targets":selected,
                                "testProducts":{name:str(executable_product_path(args.build_dir,args.config,name).resolve())
                                                for name in test_targets}}
        if before != manifest["source"]:
            raise ValueError("Build source changed during compilation")
        manifest["configuration"] = args.config
        manifest["toolchain"] = (args.build_dir / "CMakeCache.txt").read_text(encoding="utf-8")
        manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    verify(args.source, json.loads(manifest_path.read_text(encoding="utf-8")))
    print(json.dumps({"status": "passed", "manifest": str(manifest_path)}))


if __name__ == "__main__":
    main()
