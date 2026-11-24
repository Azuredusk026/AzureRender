"""Bounded maintenance for the engine-owned Vulkan pipeline cache pool."""
from contextlib import contextmanager
from itertools import islice
import os
from pathlib import Path
import re
import subprocess
import time

from artifact_maintenance import real_path, write_json

OWNED = re.compile(r'vk1-[0-9a-f]{1,8}-[0-9a-f]{1,8}-[0-9a-f]{1,8}-[0-9a-f]{32}(-[0-9a-f]{16})?\.azpc')


@contextmanager
def pool_lock(root):
    stream = None
    locked = False
    try:
        stream = real_path(Path(root)/'.pool.lock').open('a+b')
        stream.seek(0)
        if os.name == 'nt':
            import msvcrt
            msvcrt.locking(stream.fileno(), msvcrt.LK_NBLCK, 1)
        else:
            import fcntl
            fcntl.flock(stream.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        locked = True
    except OSError:
        pass
    try:
        yield locked
    finally:
        if stream:
            if locked:
                stream.seek(0)
                if os.name == 'nt':
                    import msvcrt
                    msvcrt.locking(stream.fileno(), msvcrt.LK_UNLCK, 1)
                else:
                    import fcntl
                    fcntl.flock(stream.fileno(), fcntl.LOCK_UN)
            stream.close()


def tracked_paths(build, root):
    repo = subprocess.run(['git', 'rev-parse', '--show-toplevel'], cwd=build.parent,
                          check=True, capture_output=True, timeout=5).stdout.decode().strip()
    result = subprocess.run(['git', 'ls-files', '-z', '--', str(root)], cwd=repo,
                            check=True, capture_output=True, timeout=5)
    return {Path(repo)/os.fsdecode(item) for item in result.stdout.split(b'\0') if item}


def clean(build, apply=False, tracked=None, maximum_pool=256*2**20, seconds=10):
    start = time.monotonic()
    build = real_path(build)
    root = real_path(build/'cache/vulkan')
    report = dict(status='ready', files=0, bytes=0, applied=apply, paths=[])
    if not root.is_dir():
        return report | dict(status='absent')
    # Inspect only the shared cache directory; nested repositories are protected.
    if any((parent/'.git').exists() or (parent/'.git').is_symlink()
           for parent in (root, root.parent, build)):
        return report | dict(status='protected-repository')
    with pool_lock(root) as locked:
        if not locked:
            return report | dict(status='busy')
        entries = list(islice(root.iterdir(), 65))
        if len(entries) > 64:
            return report | dict(status='entry-limit')
        if tracked is None:
            try:
                tracked = tracked_paths(build, root)
            except (OSError, subprocess.SubprocessError):
                return report | dict(status='git-unavailable')
        tracked = {Path(path).absolute() for path in tracked}
        items = []
        for path in entries:
            temporary = path.name.endswith('.tmp') and OWNED.fullmatch(path.name[:-4])
            if not temporary and not OWNED.fullmatch(path.name):
                continue
            try:
                real_path(path)
                if not path.is_file():
                    return report | dict(status='unsafe-entry')
                stat = path.stat()
            except (OSError, ValueError):
                return report | dict(status='unsafe-entry')
            items.append(dict(path=path, size=stat.st_size, modified=stat.st_mtime_ns,
                              temporary=bool(temporary), protected=path in tracked))
        items.sort(key=lambda item: (item['modified'], str(item['path'])))
        total = sum(item['size'] for item in items)
        count = len(items)
        selected = []
        now = time.time_ns()
        for item in items:
            expired = now-item['modified'] > 7*86400*10**9
            if item['protected']:
                continue
            if item['temporary'] or expired or item['size'] > 128*2**20 or total > maximum_pool or count > 8:
                selected.append(item)
                total -= item['size']
                count -= 1
                if len(selected) == 10:
                    break
        for item in selected:
            if time.monotonic()-start >= seconds:
                report['status'] = 'time-limit'
                break
            path = item['path']
            real_path(path)
            current = path.stat()
            if current.st_size != item['size'] or current.st_mtime_ns != item['modified']:
                report['status'] = 'changed-entry'
                break
            if apply:
                path.unlink()
            report['paths'].append(str(path))
            report['files'] += 1
            report['bytes'] += item['size']
        report['remainingBytes'] = sum(item['size'] for item in items)-report['bytes']
        report['remainingEntries'] = len(items)-report['files']
    report['seconds'] = time.monotonic()-start
    if apply:
        write_json(build/'maintenance/pipeline-cache-cleanup-latest.json', report)
    return report
