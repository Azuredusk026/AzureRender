"""Read-only preflight for task quotas and filesystem reserve (bytes)."""
import argparse
import json
from pathlib import Path
import shutil

GIB = 2**30
MIB = 2**20
RUN_QUOTA = 2 * GIB


def evaluate(*, total, free, used, planned, quota=RUN_QUOTA, diagnostic=False):
    if any(type(value) is not int or value < 0 for value in (total, free, used, planned, quota)) or total == 0 or free > total:
        raise ValueError('Budget values must be nonnegative integer bytes with free <= total')
    reserve = 5 * GIB if diagnostic else max(50 * GIB, (total + 9) // 10)
    reasons = []
    if used + planned > quota:
        reasons.append('run-quota')
    if diagnostic and used + planned > 64 * MIB:
        reasons.append('diagnostic-output-limit')
    if free - planned < reserve:
        reasons.append('volume-reserve')
    return dict(allowed=not reasons, reasons=reasons, totalBytes=total, freeBytes=free,
                usedBytes=used, plannedBytes=planned, quotaBytes=quota,
                reserveBytes=reserve, diagnostic=diagnostic)


def check(build, *, run=None, planned=RUN_QUOTA, quota=RUN_QUOTA, diagnostic=False):
    from artifact_cleanup import inspect_tree, is_link
    build = Path(build).absolute()
    if is_link(build) or any(is_link(p) for p in build.parents):
        raise ValueError('Link in build path')
    used = 0
    if run is not None:
        run = Path(run).absolute()
        runs = build / 'runs'
        if run.parent != runs or is_link(runs) or is_link(run) or not run.is_dir():
            raise ValueError('Run must be a real direct child of build/runs')
        used, _ = inspect_tree(run)
    usage = shutil.disk_usage(build)
    return evaluate(total=usage[0], free=usage[2], used=used, planned=planned,
                    quota=quota, diagnostic=diagnostic)


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--build', type=Path, default=Path(__file__).resolve().parents[1]/'build')
    parser.add_argument('--run', type=Path)
    parser.add_argument('--planned-bytes', type=int, default=RUN_QUOTA)
    parser.add_argument('--diagnostic', action='store_true')
    args = parser.parse_args()
    try:
        result = check(args.build, run=args.run, planned=args.planned_bytes, diagnostic=args.diagnostic)
    except (ValueError, OSError) as error:
        parser.error(str(error))
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result['allowed'] else 2


if __name__ == '__main__':
    raise SystemExit(main())
