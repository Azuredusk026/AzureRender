"""Bounded, scope-aware text queries for local development (Python 3.10+)."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import time

ENGINE = Path(__file__).resolve().parents[1]
CONFIG = Path(__file__).with_name('code_query_scopes.json')
MAX_FILE_BYTES = 4 * 1024 * 1024
TEXT_SUFFIXES = {'.cpp', '.hpp', '.h', '.c', '.cc', '.cs', '.py', '.lua', '.glsl', '.frag', '.vert', '.comp', '.geom', '.inc', '.md', '.json', '.txt', '.yml', '.yaml', '.cmake', '.ps1', '.sh', '.bat'}


class QueryError(ValueError):
    pass


class Parser(argparse.ArgumentParser):
    def error(self, message):
        raise QueryError(message)


def safe_path(root, name, validated=None):
    root = Path(root).absolute()
    supplied = Path(name)
    if supplied.is_absolute() or supplied.drive or '..' in supplied.parts:
        raise QueryError('Path must be relative and stay inside the engine root')
    lexical = root / supplied
    if lexical != root and root not in lexical.parents:
        raise QueryError('Path is outside the engine root')
    current = root
    for part in supplied.parts:
        current = current / part
        if validated is not None and current in validated:
            continue
        try:
            info = current.lstat()
        except FileNotFoundError:
            continue
        if stat.S_ISLNK(info.st_mode) or getattr(info, 'st_file_attributes', 0) & 1024:
            raise QueryError('Symlink/reparse point is outside the query contract')
        if validated is not None and current != lexical:
            validated.add(current)
    return lexical


def scope_paths(root, scope, module=None):
    config = json.loads(CONFIG.read_text(encoding='utf-8'))
    if config.get('schemaVersion') != 1 or scope not in config['scopes']:
        raise QueryError('Unknown scope or unsupported scope configuration')
    paths = [safe_path(root, name) for name in config['scopes'][scope]]
    if module:
        selected = safe_path(root, module)
        if not any(selected == p or p in selected.parents for p in paths):
            raise QueryError('Module must be inside the selected scope')
        paths = [selected]
    return paths


def run_process(args, root, deadline, **kwargs):
    remaining = deadline - time.monotonic()
    if remaining <= 0:
        raise QueryError('Query deadline exceeded')
    try:
        return subprocess.run(args, cwd=root, capture_output=True, timeout=remaining, **kwargs)
    except subprocess.TimeoutExpired as error:
        raise QueryError('Query deadline exceeded') from error


def source_files(root, paths, deadline):
    """Use Git's ignore contract, including untracked files, with bounded scopes."""
    relative = [p.relative_to(root).as_posix() for p in paths if p.exists()]
    if not relative:
        return []
    result = run_process(['git', '-c', 'core.quotePath=false', 'ls-files', '-z', '--cached', '--others', '--exclude-standard', '--', *relative], root, deadline)
    if result.returncode:
        raise QueryError('Git inventory failed: ' + result.stderr.decode('utf-8', 'replace')[:200])
    # Git's paths are relative to cwd, even when root is a subdirectory of the repo.
    names = sorted(set(n for n in result.stdout.decode('utf-8').split('\0') if n))
    files = []
    validated = set()
    for name in names:
        path = safe_path(root, name, validated)
        if path.is_file() and (path.suffix.lower() in TEXT_SUFFIXES or path.name == 'CMakeLists.txt'):
            if path.stat().st_size <= MAX_FILE_BYTES:
                files.append(path)
    return files


def base_payload(operation, scope):
    return {'schemaVersion': 1, 'operation': operation, 'scope': scope,
            'status': 'ok', 'results': [], 'truncated': False, 'elapsedMs': 0}


def search(root, query, scope, module, limit, deadline):
    if not query or '\n' in query or '\r' in query:
        raise QueryError('Use a nonempty single-line literal query')
    if scope in {'artifacts', 'private'}:
        raise QueryError('Ignored scopes support explicit read paths only')
    paths = scope_paths(root, scope, module)
    files = source_files(root, paths, deadline)
    payload = base_payload('search', scope)
    payload['maxFileBytes'] = MAX_FILE_BYTES
    # Keep Windows command lines short. rg returns only matching paths; source
    # lines are read from a current, size-limited snapshot for stable fingerprints.
    for offset in range(0, len(files), 120):
        batch = files[offset:offset+120]
        result = run_process(['rg', '--no-config', '--files-with-matches', '--null', '--fixed-strings', '--max-filesize', str(MAX_FILE_BYTES), '--', query, *[p.relative_to(root).as_posix() for p in batch]], root, deadline)
        if result.returncode not in (0, 1):
            raise QueryError('ripgrep failed: ' + result.stderr.decode('utf-8', 'replace')[:200])
        matches = {n.replace('\\', '/') for n in result.stdout.decode('utf-8').split('\0') if n}
        for path in batch:
            name = path.relative_to(root).as_posix()
            if name not in matches:
                continue
            raw = path.read_bytes()
            if len(raw) > MAX_FILE_BYTES:
                raise QueryError('File grew beyond snapshot limit')
            digest = hashlib.sha256(raw).hexdigest()
            for number, line in enumerate(raw.decode('utf-8', 'replace').splitlines(), 1):
                at = line.find(query)
                if at < 0:
                    continue
                if len(payload['results']) == limit:
                    payload['truncated'] = True
                    return payload
                payload['results'].append({'path': name, 'line': number, 'text': line[max(0, at-100):at+len(query)+160][:600], 'sha256': digest})
    return payload


def read(root, name, scope, start=1, lines=80):
    if start < 1 or not 1 <= lines <= 500:
        raise QueryError('Start must be positive and lines must be in 1..500')
    path = safe_path(root, name)
    if not any(path == p or p in path.parents for p in scope_paths(root, scope)):
        raise QueryError('Read path must be inside the selected scope')
    if not path.is_file() or path.stat().st_size > MAX_FILE_BYTES:
        raise QueryError('File is missing or larger than the 4 MiB snapshot limit')
    raw = path.read_bytes()
    if len(raw) > MAX_FILE_BYTES:
        raise QueryError('File grew beyond snapshot limit')
    content = raw.decode('utf-8', 'replace').splitlines()
    selected = content[start-1:start-1+lines]
    payload = base_payload('read', scope)
    if selected:
        payload['results'] = [{'path': path.relative_to(root).as_posix(), 'start': start,
                               'end': start+len(selected)-1, 'lines': selected,
                               'sha256': hashlib.sha256(raw).hexdigest()}]
    return payload


def serialize(payload):
    return json.dumps(payload, ensure_ascii=False, separators=(',', ':')) + '\n'


def bounded_output(payload, max_chars):
    """Budget the entire JSON response, including metadata and trailing newline."""
    if payload['truncated']:
        payload.setdefault('suggestion', 'Narrow the module/query or request fewer lines.')
    while len(serialize(payload)) > max_chars:
        payload['truncated'] = True
        payload['suggestion'] = 'Narrow the module/query or request fewer lines.'
        rows = payload['results']
        if len(rows) > 1:
            rows.pop()
        elif rows and 'lines' in rows[0] and len(rows[0]['lines']) > 1:
            rows[0]['lines'].pop()
            rows[0]['end'] -= 1
        elif rows:
            row = rows[0]
            text = row['lines'][0] if 'lines' in row else row.get('text', '')
            if len(text) > 20:
                if 'lines' in row:
                    row['lines'][0] = text[:len(text)//2]
                else:
                    row['text'] = text[:len(text)//2]
                row['partialLine'] = True
            else:
                rows.clear()
        else:
            payload.pop('error', None)
            if len(serialize(payload)) > max_chars:
                raise QueryError('Output budget is too small for metadata')
    return serialize(payload)


def parser():
    p = Parser(description=__doc__)
    p.add_argument('--root', type=Path, default=ENGINE)
    sub = p.add_subparsers(dest='operation', required=True, parser_class=Parser)
    for name in ('search', 'read'):
        command = sub.add_parser(name)
        command.add_argument('query' if name == 'search' else 'path')
        command.add_argument('--scope', default='source')
        command.add_argument('--max-chars', type=int, default=12000)
        command.add_argument('--json', action='store_true', help='Machine-readable JSON (also the default)')
        command.add_argument('--timeout', type=float, default=10)
        if name == 'search':
            command.add_argument('--module')
            command.add_argument('--limit', type=int, default=20)
        else:
            command.add_argument('--start', type=int, default=1)
            command.add_argument('--lines', type=int, default=80)
    return p


def main(argv=None):
    started = time.monotonic()
    budget = 12000
    try:
        args = parser().parse_args(argv)
        budget = args.max_chars
        if not 700 <= budget <= 12000 or not 0 < args.timeout <= 30:
            raise QueryError('Budget must be in 700..12000 and timeout in (0,30]')
        root = args.root.resolve()
        if args.operation == 'search':
            if not 1 <= args.limit <= 20:
                raise QueryError('Limit must be in 1..20')
            payload = search(root, args.query, args.scope, args.module, args.limit, started+args.timeout)
        else:
            payload = read(root, args.path, args.scope, args.start, args.lines)
        payload['elapsedMs'] = round((time.monotonic()-started)*1000, 2)
        found = bool(payload['results'])
        output = bounded_output(payload, budget)
        sys.stdout.write(output)
        return 0 if found else 1
    except (QueryError, OSError, UnicodeError, json.JSONDecodeError) as error:
        payload = base_payload('error', '')
        payload.update(status='error', error=str(error)[:240])
        sys.stdout.write(bounded_output(payload, max(700, min(budget, 12000))))
        return 2


if __name__ == '__main__':
    sys.stdout.reconfigure(encoding='utf-8')
    raise SystemExit(main())
