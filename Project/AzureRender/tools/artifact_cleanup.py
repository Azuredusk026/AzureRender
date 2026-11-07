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
    parser.add_argument("--run-list", type=Path, help="JSON array of explicit direct run paths for a reviewed batch")
    parser.add_argument("--new-run", metavar="TASK")
    parser.add_argument("--planned-bytes", type=int, default=2**31)
    parser.add_argument("--diagnostic", action="store_true")
    parser.add_argument("--input-run", action="append", default=[], help="Lease a direct input task until this task closes")
    parser.add_argument("--retention-days",type=int,choices=[1,3,7],default=1)
    parser.add_argument("--retain-recovery",action="store_true",help="Explicitly retain complete recovery for a unique required input")
    parser.add_argument("--finish", type=Path)
    args = parser.parse_args()
    runs = BUILD / "runs"
    if any(is_link(p) for p in (BUILD, *BUILD.parents, runs) if p.exists() or p.is_symlink()):
        parser.error('Link in artifact output path')
    runs.mkdir(parents=True, exist_ok=True)
    if args.new_run:
        if not re.fullmatch(r"[a-z0-9]+(?:-[a-z0-9]+)*", args.new_run):
            parser.error("TASK must be lowercase words separated by hyphens")
        from artifact_budget import check
        try:
            budget = check(BUILD, planned=args.planned_bytes, diagnostic=args.diagnostic)
        except (ValueError, OSError) as error:
            parser.error(str(error))
        if not budget['allowed']:
            parser.error('Artifact budget refused: ' + json.dumps(budget))
        path = runs / (time.strftime("%Y%m%d-%H%M%S-") + args.new_run + "-" + uuid.uuid4().hex[:6])
        from artifact_maintenance import create_run
        try:
            create_run(BUILD, path, budget, args.input_run,retention_days=args.retention_days,retain_recovery=args.retain_recovery)
        except (ValueError, OSError) as error:
            parser.error(str(error))
        print(path)
        return
    if args.finish:
        path = args.finish.resolve()
        if path.parent != runs.resolve() or is_link(path):
            parser.error("--finish requires a direct child of build/runs")
        from artifact_maintenance import seal
        try:
            seal(BUILD, path)
        except (ValueError, OSError) as error:
            parser.error(str(error))
        (path / ".finished").touch()
        return
    repo = Path(subprocess.check_output(["git", "rev-parse", "--show-toplevel"], cwd=ENGINE, text=True).strip())
    tracked = { (repo / p).resolve() for p in subprocess.check_output(
        ["git", "ls-files", "-z"], cwd=repo).decode("utf-8").split("\0") if p }
    now = time.time()
    paths = [p for p in runs.iterdir() if eligible(p, RETENTION_DAYS, now)]
    if args.run_list:
        selected = json.loads(args.run_list.read_text(encoding='utf-8'))
        if not isinstance(selected, list) or not selected or any(not isinstance(p, str) for p in selected):
            parser.error('--run-list requires a nonempty JSON array of paths')
        requested = {Path(p).absolute() for p in selected}
        available = {p.absolute() for p in paths}
        if not requested <= available:
            parser.error('--run-list contains an ineligible or out-of-scope run')
        paths = [p for p in paths if p.absolute() in requested]
    if args.legacy_list:
        for name in json.loads(args.legacy_list.read_text(encoding="utf-8")):
            if not re.fullmatch(r"[a-z0-9][a-z0-9-]*", name):
                parser.error("Legacy names must be direct build children")
            if name.startswith("ninja") or name in {"qa-python", "runs", "archives", "current", "ux-2026-10-14", "p8-final-2026-10-14"}:
                parser.error(f"Protected directory: {name}")
            p = BUILD / name
            if p.exists():
                paths.append(p)
    paths = list(dict.fromkeys(paths))
    if args.apply and len(paths) > 10:
        parser.error('Deletion batch exceeds 10 directories; use a reviewed --run-list')
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
