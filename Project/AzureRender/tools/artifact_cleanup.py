"""Bounded cleanup for AzureRender generated artifacts; dry-run by default."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import time
import uuid

ENGINE = Path(__file__).resolve().parents[1]
BUILD = ENGINE / "build"
RETENTION_DAYS = 7

def is_link(path):
    return path.is_symlink() or bool(getattr(path.lstat(), "st_file_attributes", 0) & 1024)

def inspect_tree(path):
    if is_link(path):
        raise ValueError(f"Link/reparse point: {path}")
    total = 0
    newest = path.stat().st_mtime
    if path.is_file():
        return path.stat().st_size, newest
    pending = [path]
    while pending:
        with os.scandir(pending.pop()) as entries:
            for entry in entries:
                info = entry.stat(follow_symlinks=False)
                if entry.is_symlink() or getattr(info, "st_file_attributes", 0) & 1024:
                    raise ValueError(f"Link/reparse point: {entry.path}")
                newest = max(newest, info.st_mtime)
                if entry.is_dir(follow_symlinks=False):
                    pending.append(entry.path)
                else:
                    total += info.st_size
    return total, newest

def eligible(path, days, now):
    marker = path / ".finished"
    return (path.is_dir() and not is_link(path) and marker.is_file()
            and not (path / ".keep").exists()
            and now - marker.stat().st_mtime >= days * 86400)

def remove_artifact(root, path, tracked, *, apply=False):
    if is_link(root):
        raise ValueError(f"Link cleanup root: {root}")
    root = root.resolve()
    lexical = path.absolute()
    resolved = path.resolve()
    if resolved == root or root not in resolved.parents:
        raise ValueError(f"Outside cleanup root: {path}")
    for parent in [lexical, *lexical.parents]:
        if parent == root:
            break
        if is_link(parent):
            raise ValueError(f"Link ancestor: {parent}")
    if any(p == resolved or resolved in p.parents for p in tracked):
        raise ValueError(f"Tracked content: {path}")
    size, newest = inspect_tree(path)
    if apply:
        if path.is_dir():
            def retry_readonly(function, failed_path, error):
                item = Path(failed_path)
                if (not isinstance(error[1], PermissionError) or is_link(item)
                        or item.stat().st_mode & stat.S_IWRITE):
                    raise error[1]
                os.chmod(item, stat.S_IWRITE | stat.S_IREAD)
                function(failed_path)
            shutil.rmtree(path, onerror=retry_readonly)
        else:
            path.unlink()
    return size, newest

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apply", action="store_true")
    parser.add_argument("--legacy-list", type=Path)
    parser.add_argument("--new-run", metavar="TASK")
    parser.add_argument("--finish", type=Path)
    args = parser.parse_args()
    runs = BUILD / "runs"
    runs.mkdir(parents=True, exist_ok=True)
    if args.new_run:
        if not re.fullmatch(r"[a-z0-9]+(?:-[a-z0-9]+)*", args.new_run):
            parser.error("TASK must be lowercase words separated by hyphens")
        path = runs / (time.strftime("%Y%m%d-%H%M%S-") + args.new_run + "-" + uuid.uuid4().hex[:6])
        path.mkdir()
        print(path)
        return
    if args.finish:
        path = args.finish.resolve()
        if path.parent != runs.resolve() or is_link(path):
            parser.error("--finish requires a direct child of build/runs")
        (path / ".finished").touch()
        return
    repo = Path(subprocess.check_output(["git", "rev-parse", "--show-toplevel"], cwd=ENGINE, text=True).strip())
    tracked = { (repo / p).resolve() for p in subprocess.check_output(
        ["git", "ls-files", "-z"], cwd=repo).decode("utf-8").split("\0") if p }
    now = time.time()
    paths = [p for p in runs.iterdir() if eligible(p, RETENTION_DAYS, now)]
    if args.legacy_list:
        for name in json.loads(args.legacy_list.read_text(encoding="utf-8")):
            if not re.fullmatch(r"[a-z0-9][a-z0-9-]*", name):
                parser.error("Legacy names must be direct build children")
            if name.startswith("ninja") or name in {"qa-python", "runs", "archives", "current", "ux-2026-10-14", "p8-final-2026-10-14"}:
                parser.error(f"Protected directory: {name}")
            p = BUILD / name
            if p.exists():
                paths.append(p)
    report = {"time": time.strftime("%Y-%m-%dT%H:%M:%S"), "applied": args.apply, "items": [], "bytes": 0}
    receipt = BUILD / "maintenance"
    receipt.mkdir(exist_ok=True)
    for path in paths:
        try:
            size, newest = remove_artifact(BUILD, path, tracked)
            if path.parent == runs and now - newest < RETENTION_DAYS * 86400:
                continue
            if (path / ".keep").exists():
                continue
            if args.apply:
                remove_artifact(BUILD, path, tracked, apply=True)
            report["items"].append({"path": str(path), "bytes": size})
            report["bytes"] += size
            print(f"{path.name}: {size} bytes", flush=True)
        except (OSError, ValueError) as error:
            report["items"].append({"path": str(path), "error": str(error)})
            print(f"Skipped {path}: {error}", flush=True)
        (receipt / "cleanup-latest.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    (receipt / "cleanup-latest.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({"bytes": report["bytes"], "items": len(report["items"])}))
    if any("error" in item for item in report["items"]):
        raise SystemExit(1)

if __name__ == "__main__":
    main()
