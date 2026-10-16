"""Bounded offline reader diagnostics. Hook discovery never proves asset parsing."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


class ProbeError(Exception):
    def __init__(self, code, message):
        super().__init__(message)
        self.code = code


def fingerprint(path):
    with path.open('rb') as stream:
        digest = hashlib.file_digest(stream, 'sha256').hexdigest()
    return {'path': str(path), 'bytes': path.stat().st_size, 'sha256': digest}


def preflight(root):
    for name in ('Ruri.RipperHook.dll', 'Ruri.RipperHook.CLI.runtimeconfig.json'):
        if not (root / name).is_file():
            raise ProbeError('missing_file', name)
    config = json.loads((root / 'Ruri.RipperHook.CLI.runtimeconfig.json').read_text(encoding='utf-8-sig'))
    if config.get('runtimeOptions', {}).get('tfm') != 'net10.0':
        raise ProbeError('unsupported_framework', 'Expected net10.0')
    deps = root / 'Ruri.RipperHook.CLI.deps.json'
    if not deps.is_file():
        raise ProbeError('missing_file', deps.name)
    for target in json.loads(deps.read_text(encoding='utf-8-sig')).get('targets', {}).values():
        for package in target.values():
            for name in package.get('runtime', {}):
                if name.endswith('.dll') and not (root / Path(name).name).is_file():
                    raise ProbeError('missing_dependency', name)
    if not (root / 'Ruri.RipperHook.CLI.exe').is_file():
        raise ProbeError('missing_file', 'Ruri.RipperHook.CLI.exe')


def run_probe(args):
    root = args.bin_dir.resolve()
    report = {'schema_version': 1, 'bin_dir': str(root), 'game_root': str(args.game_root.resolve()),
              'resource_read_verified': False, 'observations': [], 'unknowns': ['Asset parsing and dependency closure require a verified nonempty export.']}
    try:
        preflight(root)
        report['files'] = [fingerprint(p) for p in sorted(root.rglob('*')) if p.is_file()]
        command = [str(root / 'Ruri.RipperHook.CLI.exe'), '--list-hooks']
        result = subprocess.run(command, cwd=args.output.resolve().parent, capture_output=True,
                                text=True, errors='replace', timeout=args.timeout)
        report['hook_discovery'] = {'command': command, 'exit_code': result.returncode,
                                    'stdout': result.stdout, 'stderr': result.stderr}
        # The upstream discovery command intentionally returns 3 on success.
        hook_record = next((json.loads(line) for line in result.stdout.splitlines()
                           if line.startswith('{') and 'list_hooks' in line), None)
        if result.returncode not in (0, 3) or not hook_record or hook_record.get('status') != 'ok':
            raise ProbeError('hook_discovery_failed', 'See hook_discovery logs')
        report['hooks'] = hook_record['hooks']
        report['status'] = 'discovery_only'
        return report, 0
    except subprocess.TimeoutExpired:
        report['error'] = {'code': 'timeout', 'message': 'Reader exceeded process time budget'}
    except (ProbeError, OSError, ValueError) as error:
        report['error'] = {'code': getattr(error, 'code', 'invalid_input'), 'message': str(error)}
    report['status'] = 'failed'
    return report, 2


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bin-dir', required=True, type=Path)
    parser.add_argument('--game-root', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--timeout', type=float, default=30)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    report, code = run_probe(args)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    return code


if __name__ == '__main__':
    raise SystemExit(main())
