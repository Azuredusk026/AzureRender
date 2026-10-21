"""Capture and inspect explicit targets through an external RenderDoc Python runtime."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import subprocess


def fingerprint(path):
    path = Path(path).resolve()
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return {'path': str(path), 'bytes': path.stat().st_size, 'sha256': digest.hexdigest()}


def _check_file(record, *, inside=None):
    if not isinstance(record, dict) or not isinstance(record.get('path'), str):
        raise ValueError('Missing artifact identity')
    path = Path(record['path']).resolve()
    if inside is not None:
        try:
            path.relative_to(Path(inside).resolve())
        except ValueError:
            raise ValueError('Artifact is outside the selected output directory')
    if not path.is_file():
        raise ValueError('Missing artifact: {}'.format(path))
    actual = fingerprint(path)
    if (actual['bytes'] <= 0 or type(record.get('bytes')) is not int
            or record['bytes'] != actual['bytes'] or record.get('sha256') != actual['sha256']):
        raise ValueError('Artifact digest or size does not match: {}'.format(path))
    return path


def verify_receipt(receipt, operation, source, output):
    """Reject success without matching source identity and retained output files."""
    if (not isinstance(receipt, dict) or receipt.get('format') != 'RenderDocEvidence/1'
            or receipt.get('operation') != operation):
        raise ValueError('Unexpected RenderDoc evidence format or operation')
    expected_status = 'replayed' if operation == 'replay' else 'captured'
    if receipt.get('status') != expected_status:
        raise ValueError('RenderDoc operation did not succeed')
    if not isinstance(receipt.get('tool_version'), str) or not receipt['tool_version']:
        raise ValueError('Missing actual RenderDoc version')
    if _check_file(receipt.get('source')) != Path(source).resolve():
        raise ValueError('Evidence refers to a different source')
    artifacts = receipt.get('artifacts')
    if not isinstance(artifacts, list) or not artifacts:
        raise ValueError('No retained output artifacts')
    paths = [_check_file(record, inside=output) for record in artifacts]
    if len(set(paths)) != len(paths):
        raise ValueError('Duplicate artifact path')
    if operation == 'replay':
        if not isinstance(receipt.get('api'), str) or 'Unknown' in receipt['api'] or not receipt['api']:
            raise ValueError('Missing actual graphics API')
        for field in ('action_count', 'draw_count', 'dispatch_count'):
            if type(receipt.get(field)) is not int or receipt[field] < (1 if field == 'action_count' else 0):
                raise ValueError('Invalid replay action counts')
    else:
        capture = _check_file(receipt.get('capture'), inside=output)
        if capture not in paths:
            raise ValueError('Capture is absent from retained artifacts')
        if type(receipt.get('pid')) is not int or receipt['pid'] <= 0:
            raise ValueError('Missing observed target process')


def run_evidence(operation, *, python, renderdoc_dir, source, output, arguments=(),
                 frame=3, events=(), timeout=120, pid=None, working_dir=None):
    """Run the official API in a caller-selected compatible Python interpreter.

    This driver terminates only its worker on timeout. Target application lifetime
    belongs to the caller. It never retries process injection or discovers PIDs.
    """
    output = Path(output).resolve()
    if output.exists():
        raise ValueError('Output directory already exists: {}'.format(output))
    if operation not in ('capture', 'inject', 'replay'):
        raise ValueError('Unknown RenderDoc operation')
    python, source, renderdoc_dir = Path(python).resolve(), Path(source).resolve(), Path(renderdoc_dir).resolve()
    for path in (python, source, renderdoc_dir/'renderdoc.pyd', renderdoc_dir/'renderdoc.dll'):
        if not path.is_file():
            raise ValueError('Required input is missing: {}'.format(path))
    if type(timeout) not in (int, float) or not math.isfinite(timeout) or not 0 < timeout <= 3600:
        raise ValueError('Timeout must be finite and in (0, 3600] seconds')
    if type(frame) is not int or frame < 1:
        raise ValueError('Capture frame must be a positive integer')
    if not all(type(event) is int and event > 0 for event in events) or len(set(events)) != len(events):
        raise ValueError('Replay events must be distinct positive integers')
    if not all(isinstance(argument, str) for argument in arguments):
        raise ValueError('Target arguments must be strings')
    if operation == 'inject' and (type(pid) is not int or pid <= 0):
        raise ValueError('Injection requires an explicit positive PID')
    directory = Path(working_dir).resolve() if working_dir else source.parent
    if not directory.is_dir():
        raise ValueError('Working directory is missing')
    request = {'format': 'RenderDocRequest/1', 'operation': operation,
        'source': fingerprint(source), 'output': str(output), 'renderdoc_dir': str(renderdoc_dir),
        'arguments': list(arguments), 'frame': frame, 'events': list(events),
        'timeout': timeout, 'pid': pid, 'working_directory': str(directory)}
    driver = {'python': fingerprint(python), 'renderdoc_dll': fingerprint(renderdoc_dir/'renderdoc.dll'),
              'renderdoc_pyd': fingerprint(renderdoc_dir/'renderdoc.pyd')}
    output.mkdir(parents=True)
    request_path = output/'request.json'
    request_path.write_text(json.dumps(request, indent=2), encoding='utf8')
    receipt_path = output/'receipt.json'
    receipt = {'format': 'RenderDocEvidence/1', 'operation': operation,
               'status': 'failed', 'source': request['source'], 'artifacts': []}
    try:
        worker = Path(__file__).with_name('renderdoc_worker.py')
        with (output/'worker.log').open('w', encoding='utf8') as log:
            completed = subprocess.run([str(python), str(worker), str(request_path)],
                stdout=log, stderr=subprocess.STDOUT, timeout=timeout)
        if receipt_path.is_file():
            produced = json.loads(receipt_path.read_text(encoding='utf8'))
            if not isinstance(produced, dict):
                raise ValueError('Worker receipt must be an object')
            receipt = produced
        if completed.returncode:
            raise ValueError('RenderDoc worker failed with exit code {}'.format(completed.returncode))
        verify_receipt(receipt, operation, source, output)
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        receipt['status'] = 'failed'
        receipt['driver_error'] = str(error)
    receipt['driver'] = driver
    receipt['target_lifetime'] = 'caller-owned; no target termination is performed'
    receipt_path.write_text(json.dumps(receipt, indent=2), encoding='utf8')
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='operation', required=True)
    for operation in ('capture', 'inject', 'replay'):
        command = commands.add_parser(operation)
        command.add_argument('--python', required=True, type=Path,
            help='Python interpreter compatible with the selected renderdoc.pyd')
        command.add_argument('--renderdoc-dir', required=True, type=Path)
        command.add_argument('--output', required=True, type=Path)
        command.add_argument('--timeout', type=float, default=120)
        if operation == 'replay':
            command.add_argument('--capture', required=True, type=Path, dest='source')
            command.add_argument('--event', action='append', type=int, dest='events', default=[])
        else:
            command.add_argument('--executable', required=True, type=Path, dest='source')
            if operation == 'inject':
                command.add_argument('--pid', required=True, type=int)
            else:
                command.add_argument('--working-dir', type=Path)
                command.add_argument('--frame', type=int, default=3)
                command.add_argument('arguments', nargs=argparse.REMAINDER)
    args = vars(parser.parse_args())
    if args.get('arguments', [])[:1] == ['--']:
        args['arguments'] = args['arguments'][1:]
    try:
        receipt = run_evidence(**args)
    except ValueError as error:
        parser.error(str(error))
    print(json.dumps({key: receipt.get(key) for key in
        ('operation', 'status', 'tool_version', 'api', 'action_count', 'draw_count', 'driver_error')}, indent=2))
    if receipt['status'] == 'failed':
        raise SystemExit(1)


if __name__ == '__main__':
    main()
