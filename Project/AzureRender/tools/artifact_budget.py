"""Check the filesystem reserve and record expected task output (bytes)."""
import argparse
import json
from pathlib import Path
import shutil

GIB = 2**30
MIB = 2**20
DEFAULT_PLANNED_BYTES = 2 * GIB
VOLUME_MIN_FREE = 200 * GIB
VOLUME_TARGET_FREE = 300 * GIB


def volume_reserve(total):
    return max(VOLUME_MIN_FREE, (total + 9) // 10)


def evaluate(*, total, free, used, planned, quota=None, diagnostic=False):
    if any(type(value) is not int or value < 0 for value in (total, free, used, planned)) or total == 0 or free > total:
        raise ValueError('Budget values must be nonnegative integer bytes with free <= total')
    reserve = 5 * GIB if diagnostic else volume_reserve(total)
    reasons = []
    if quota is not None and (type(quota) is not int or quota < 0):
        raise ValueError('Explicit quota must be nonnegative integer bytes')
    if quota is not None and used + planned > quota:
        reasons.append('run-quota')
    if diagnostic and used + planned > 64 * MIB:
        reasons.append('diagnostic-output-limit')
    if free - planned < reserve:
        reasons.append('volume-reserve')
    return dict(allowed=not reasons, reasons=reasons, totalBytes=total, freeBytes=free,
                usedBytes=used, plannedBytes=planned, quotaBytes=quota,
                reserveBytes=reserve, diagnostic=diagnostic)


def check(build, *, run=None, planned=DEFAULT_PLANNED_BYTES, quota=None, diagnostic=False):
    from artifact_cleanup import is_link
    build = Path(build).absolute()
    if is_link(build) or any(is_link(p) for p in build.parents):
        raise ValueError('Link in build path')
    used = 0
    if run is not None:
        run = Path(run).absolute()
        runs = build / 'runs'
        if run.parent != runs or is_link(runs) or is_link(run) or not run.is_dir():
            raise ValueError('Run must be a real direct child of build/runs')
        from artifact_maintenance import measure
        used = measure(run)
    usage = shutil.disk_usage(build)
    result = evaluate(total=usage[0], free=usage[2], used=used, planned=planned,
                      quota=quota, diagnostic=diagnostic)
    from artifact_maintenance import pool_budget
    pool = pool_budget(build, run=run, used=used, planned=planned)
    result['managedPool'] = pool
    if not pool['allowed']:
        result['allowed'] = False
        result['reasons'].extend(pool['reasons'])
    elif run is not None and result['allowed']:
        from artifact_maintenance import reserve_write
        result['managedPool'] = reserve_write(build, run, used, planned)
        if not result['managedPool']['allowed']:
            result['allowed'] = False
            result['reasons'].extend(result['managedPool']['reasons'])
    return result


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--build', type=Path, default=Path(__file__).resolve().parents[1]/'build')
    parser.add_argument('--run', type=Path)
    parser.add_argument('--planned-bytes', type=int, default=DEFAULT_PLANNED_BYTES)
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
