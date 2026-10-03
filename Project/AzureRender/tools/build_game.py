"""Build a movable Windows game directory from a project and Release install tree."""
from __future__ import annotations
import argparse
import hashlib
import json
import pathlib
import shutil
import subprocess
import tempfile
import uuid

REQUIRED_LICENSES = [name + "-LICENSE.txt" for name in [
    "glfw3", "tinygltf", "stb", "openexr", "imath", "libdeflate", "openjph", "nlohmann-json",
    "joltphysics", "lua", "sol2", "rmlui", "miniaudio", "freetype", "libpng", "zlib", "bzip2", "brotli", "LatoLatin"]]
PRIVATE_NAMES = {"assets_private", "captures", "portfolio"}
GENERATED_NAMES = {".azure", ".git", "__pycache__"}


def read_json(path):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (ValueError, OSError) as error:
        raise ValueError(f"Cannot read {path}: {error}") from error


def inside(root, relative):
    path = pathlib.Path(relative)
    if not relative or path.is_absolute() or path.drive or ":" in str(relative):
        raise ValueError(f"Expected a relative package path: {relative}")
    result = (root / path).resolve()
    if not result.is_relative_to(root.resolve()):
        raise ValueError(f"Path escapes its root: {relative}")
    return result


def is_reparse(path):
    return path.is_symlink() or bool(getattr(path.lstat(), "st_file_attributes", 0) & 0x400)


def copy_tree(source, destination):
    if source.name in PRIVATE_NAMES:
        raise ValueError(f"Private or generated export boundary: {source}")
    if is_reparse(source):
        raise ValueError(f"Package source must be a regular directory: {source}")
    destination.mkdir(parents=True, exist_ok=True)
    for item in sorted(source.iterdir()):
        if item.name in GENERATED_NAMES or item.name.endswith(".tmp"):
            continue
        if item.name in PRIVATE_NAMES:
            raise ValueError(f"Private or generated export boundary: {item}")
        if is_reparse(item):
            raise ValueError(f"Package source contains a link: {item}")
        target = destination / item.name
        if item.is_dir():
            copy_tree(item, target)
        else:
            shutil.copy2(item, target)


def validate_project(player, project):
    result = subprocess.run([str(player), "--project", str(project), "--check-project"],
        cwd=project.parent, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=60)
    if result.returncode:
        raise ValueError("Project validation failed:\n" + result.stdout + result.stderr)


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def verify_package(directory):
    directory = pathlib.Path(directory).resolve()
    manifest = read_json(directory / "game_manifest.json")
    if manifest.get("format") != "AzureGame v1" or manifest.get("schemaVersion") != 1:
        raise ValueError("Unsupported game package manifest")
    paths = set()
    for entry in manifest["files"]:
        name = entry["path"]
        if name in paths:
            raise ValueError(f"Duplicate manifest entry: {name}")
        paths.add(name)
        file = inside(directory, name)
        if not file.is_file() or file.stat().st_size != entry["size"] or digest(file) != entry["sha256"]:
            raise ValueError(f"Game package hash mismatch: {name}")
    required_files = ["bin/AzurePlayer.exe", "game/project.azureproject", "start-game.cmd",
        "share/AzureRender/assets_public/fonts/LatoLatin-Regular.ttf", "share/AzureRender/runtime-build.json",
        "share/AzureRender/shaders/game-ui.vert.spv", "share/AzureRender/shaders/game-ui.frag.spv"]
    required_files += ["share/AzureRender/licenses/" + name for name in REQUIRED_LICENSES]
    for required in required_files:
        if required not in paths:
            raise ValueError(f"Game package is missing {required}")
    for file in directory.rglob("*"):
        if file.is_file() and file.relative_to(directory).as_posix() not in paths | {"game_manifest.json"}:
            relative = file.relative_to(directory)
            if relative.parts[0] != "captures" and relative.parts[:2] != ("game", ".azure"):
                raise ValueError(f"Unlisted game package file: {relative}")
    return manifest


def build_game(project, install, output, replace=False, validator=None):
    project, install = pathlib.Path(project).resolve(), pathlib.Path(install).resolve()
    requested_output = pathlib.Path(output).absolute()
    if requested_output.exists() and is_reparse(requested_output):
        raise ValueError("Package output must be a regular directory")
    output = requested_output.resolve()
    for source in [project.parent, install]:
        if output.is_relative_to(source) or source.is_relative_to(output):
            raise ValueError("Package output must be separate from its project and engine inputs")
    config = read_json(project)
    if config.get("schemaVersion") != 1 or not config.get("id") or not config.get("name"):
        raise ValueError("Expected a version 1 game project")
    metadata = read_json(install / "share/AzureRender/runtime-build.json")
    if metadata.get("configuration") != "Release" or metadata.get("architecture") != "x64":
        raise ValueError("Windows game publishing requires a Release x64 install tree")
    if output.exists():
        if not replace or not output.is_dir() or read_json(output / "game_manifest.json").get("format") != "AzureGame v1":
            raise ValueError("Output exists; replacement requires a recognized game package and --replace")
    share = install / "share/AzureRender"
    player = install / "bin/AzurePlayer.exe"
    for file in [player, share / "assets_public/fonts/LatoLatin-Regular.ttf", share / "shaders/game-ui.vert.spv", share / "shaders/game-ui.frag.spv"]:
        if not file.is_file():
            raise ValueError(f"Required engine resource is missing: {file}")
    for license_name in REQUIRED_LICENSES:
        if not (share / "licenses" / license_name).is_file():
            raise ValueError(f"Required distribution license is missing: {license_name}")
    mounts = config.get("mounts")
    if not isinstance(mounts, list) or not mounts:
        raise ValueError("Project mounts must be a nonempty list")
    names = set()
    roots = []
    for mount in mounts:
        name = mount["name"]
        if name in names or name in PRIVATE_NAMES | {"engine"} or not name or any(c not in "abcdefghijklmnopqrstuvwxyz0123456789_" for c in name):
            raise ValueError("Invalid or duplicate project mount")
        names.add(name)
        root = inside(project.parent, mount["path"])
        if any(part in PRIVATE_NAMES for part in root.relative_to(project.parent).parts):
            raise ValueError(f"Private or generated export boundary: {root}")
        roots.append(root)
    validate = validator or validate_project
    validate(player, project)
    output.parent.mkdir(parents=True, exist_ok=True)
    candidate = pathlib.Path(tempfile.mkdtemp(prefix=f".{output.name}-candidate-", dir=output.parent))
    backup = output.parent / f".{output.name}-backup-{uuid.uuid4().hex}"
    committed = False
    try:
        (candidate / "bin").mkdir()
        shutil.copy2(player, candidate / "bin/AzurePlayer.exe")
        for dll in (install / "bin").glob("*.dll"):
            if dll.name.lower() == "rmlui_debugger.dll":
                continue
            shutil.copy2(dll, candidate / "bin" / dll.name)
        game = candidate / "game"
        game.mkdir()
        for mount, source in zip(mounts, roots):
            copy_tree(source, game / mount["name"])
            mount["path"] = mount["name"]
        (game / "project.azureproject").write_text(json.dumps(config, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        for name in ["shaders", "assets_public", "licenses"]:
            copy_tree(share / name, candidate / "share/AzureRender" / name)
        shutil.copy2(share / "runtime-build.json", candidate / "share/AzureRender/runtime-build.json")
        (candidate / "start-game.cmd").write_bytes(b'@echo off\r\ncd /d "%~dp0"\r\n"bin\\AzurePlayer.exe" --project "game\\project.azureproject" --resource-root "share\\AzureRender" %*\r\n')
        validate(candidate / "bin/AzurePlayer.exe", game / "project.azureproject")
        if (game / ".azure").exists():
            shutil.rmtree(game / ".azure")
        files = [{"path": file.relative_to(candidate).as_posix(), "size": file.stat().st_size, "sha256": digest(file)}
                 for file in sorted(candidate.rglob("*")) if file.is_file()]
        manifest = {"format": "AzureGame v1", "schemaVersion": 1, "projectId": config["id"], "name": config["name"],
                    "engine": metadata, "entry": "start-game.cmd", "project": "game/project.azureproject", "files": files}
        (candidate / "game_manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        verify_package(candidate)
        if output.exists():
            output.rename(backup)
        try:
            candidate.rename(output)
        except OSError:
            if backup.exists():
                backup.rename(output)
            raise
        committed = True
        if backup.exists():
            shutil.rmtree(backup)
        return manifest
    finally:
        if not committed and candidate.exists():
            shutil.rmtree(candidate)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project", type=pathlib.Path)
    parser.add_argument("--install", type=pathlib.Path)
    parser.add_argument("--output", type=pathlib.Path)
    parser.add_argument("--replace", action="store_true")
    parser.add_argument("--verify", type=pathlib.Path)
    args = parser.parse_args()
    try:
        if args.verify:
            manifest = verify_package(args.verify)
        else:
            if not all([args.project, args.install, args.output]):
                parser.error("--project, --install and --output are required")
            manifest = build_game(args.project, args.install, args.output, args.replace)
        print(json.dumps({"status": "passed", "name": manifest["name"], "files": len(manifest["files"])}, ensure_ascii=False))
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        parser.exit(1, f"Game package: {error}\n")


if __name__ == "__main__":
    main()
