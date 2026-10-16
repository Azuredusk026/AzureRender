"""Run explicit validation commands with bounded execution and retained evidence."""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import platform
import re
import subprocess
import time


def fingerprint(path):
    with path.open('rb') as stream:
        return {'path': str(path.resolve()), 'bytes': path.stat().st_size,
                'sha256': hashlib.file_digest(stream, 'sha256').hexdigest()}


def execute(suite, output, context):
    output = output.resolve()
    steps = suite.get('steps', [])
    if not steps:
        raise ValueError('Validation suite requires at least one step')
    identities = [step['id'] for step in steps]
    if len(set(identities)) != len(identities) or any(not re.fullmatch(r'[a-zA-Z0-9_-]+', value) for value in identities):
        raise ValueError('Invalid or duplicate step id')
    for identity in identities:
        if (output / identity).exists():
            raise FileExistsError(output / identity)
    report = {'schemaVersion': 1, 'startedUtc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'host': {'platform': platform.platform(), 'machine': platform.machine()},
              'selection': context, 'source': suite.get('source', {}), 'steps': [],
              'nativeSourceVerified': False, 'passed': False}
    for step in steps:
        directory = output / step['id']
        directory.mkdir(parents=True)
        variables = {**context, 'output': str(directory)}
        argv = []
        for argument in step['argv']:
            for key, value in variables.items():
                argument = argument.replace('{' + key + '}', str(value))
            argv.append(argument)
        timeout = step.get('timeout', 120)
        if not isinstance(timeout, (float, int)) or not 0 < timeout <= 3600:
            raise ValueError('Timeout must be in (0,3600] seconds')
        entry = {'id': step['id'], 'argv': argv, 'timeoutSeconds': timeout, 'exitCode': None,
                 'status': 'failed', 'artifacts': [], 'missingFiles': []}
        executable = Path(argv[0])
        if executable.is_file():
            entry['executable'] = fingerprint(executable)
        started = time.monotonic()
        # File-backed pipes preserve partial logs and bound memory on verbose tools.
        with (directory / 'stdout.log').open('wb') as stdout, (directory / 'stderr.log').open('wb') as stderr:
            startup = None
            if hasattr(subprocess, 'STARTUPINFO'):
                startup = subprocess.STARTUPINFO()
                startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
                startup.wShowWindow = 0
            try:
                process = subprocess.Popen(argv, cwd=directory, stdout=stdout, stderr=stderr, startupinfo=startup)
                try:
                    entry['exitCode'] = process.wait(timeout=timeout)
                except subprocess.TimeoutExpired:
                    if platform.system() == 'Windows':
                        subprocess.run(['taskkill', '/PID', str(process.pid), '/T', '/F'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=10)
                    else:
                        process.kill()
                    process.wait(timeout=10)
                    entry['status'] = 'timeout'
            except OSError as error:
                entry['error'] = str(error)
        entry['elapsedSeconds'] = time.monotonic() - started
        log = (directory / 'stdout.log').read_text(encoding='utf-8', errors='replace') + (directory / 'stderr.log').read_text(encoding='utf-8', errors='replace')
        entry['missingLog'] = [value for value in step.get('requiredLog', []) if value not in log]
        entry['forbiddenLog'] = [value for value in step.get('forbiddenLog', ['VUID-', 'Validation Error']) if value in log]
        for name in step.get('requiredFiles', []):
            path = (directory / name).resolve()
            if not path.is_relative_to(directory):
                raise ValueError('Required evidence must remain inside its step directory')
            if not path.is_file() or path.stat().st_size == 0:
                entry['missingFiles'].append(name)
            else:
                entry['artifacts'].append(fingerprint(path))
        if entry['exitCode'] == 0 and not entry['missingFiles'] and not entry['missingLog'] and not entry['forbiddenLog']:
            entry['status'] = 'passed'
        report['steps'].append(entry)
        (output / 'validation.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    report['passed'] = all(step['status'] == 'passed' for step in report['steps'])
    (output / 'validation.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--suite', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    for name in ('project', 'character', 'scene', 'animation', 'expressions', 'player', 'renderer'):
        parser.add_argument('--' + name, default='')
    args = parser.parse_args()
    suite = json.loads(args.suite.read_text(encoding='utf-8'))
    suite.setdefault('source', {})['suite'] = fingerprint(args.suite)
    context = {name: getattr(args, name) for name in ('project', 'character', 'scene', 'animation', 'expressions', 'player', 'renderer')}
    for name in ('project', 'character', 'scene', 'expressions', 'player', 'renderer'):
        if context[name]:
            context[name] = str(Path(context[name]).resolve())
    revision = subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=Path(__file__).resolve().parent,
                              capture_output=True, text=True, timeout=10)
    suite['source']['gitRevision'] = revision.stdout.strip() if revision.returncode == 0 else 'unknown'
    for name in ('project', 'character', 'scene', 'expressions'):
        if context[name] and Path(context[name]).is_file():
            suite['source'][name] = fingerprint(Path(context[name]))
    return 0 if execute(suite, args.output, context)['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
