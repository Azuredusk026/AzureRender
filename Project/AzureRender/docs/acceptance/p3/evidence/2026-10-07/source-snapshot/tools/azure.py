"""Thin command entry for configured engine builds and production validation."""
import argparse
from contextlib import contextmanager
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import sys

SOURCE = Path(__file__).resolve().parents[1]
COMMANDS = ('doctor', 'build', 'run', 'test', 'describe', 'shot', 'capture', 'query', 'edit', 'validate', 'package', 'provenance')
TARGETS = ('AzureRender', 'AzurePlayer', 'AzureMetaGen', 'AzureRenderShaders')


def preset(source, name):
    data = json.loads((source / 'CMakePresets.json').read_text(encoding='utf-8'))
    for entry in data['configurePresets']:
        if entry['name'] == name:
            path = entry['binaryDir'].replace('${sourceDir}', str(source.resolve()))
            if '${' in path or '$env{' in path:
                raise ValueError('Unresolved preset binaryDir')
            return Path(path).resolve(), entry.get('cacheVariables', {}).get('CMAKE_BUILD_TYPE', 'Release')
    raise ValueError('Unknown preset: ' + name)


@contextmanager
def build_lock(directory):
    directory.mkdir(parents=True, exist_ok=True)
    file = directory / '.azure-build.lock'
    try:
        handle = file.open('x', encoding='utf-8')
    except FileExistsError as error:
        raise RuntimeError('Configuration is locked: ' + str(file)) from error
    try:
        with handle:
            json.dump({'pid': os.getpid()}, handle)
        yield
    finally:
        file.unlink()


def execute(command, cwd, json_output=False):
    return subprocess.run(list(map(str, command)), cwd=cwd, stdout=sys.stderr if json_output else None).returncode


def runtime_command(build, config, target, arguments):
    if target not in ('AzureRender', 'AzurePlayer', 'AzureMetaGen'):
        raise ValueError('Unknown runtime target: ' + target)
    directory = build / config if (build / config).is_dir() else build
    suffix = '.exe' if os.name == 'nt' else ''
    return [str(directory / (target + suffix)), *arguments]


def control(endpoint_file, token_environment, request):
    endpoint = json.loads(Path(endpoint_file).read_text(encoding='utf-8'))
    if endpoint.get('address') != '127.0.0.1' or not 1 <= endpoint.get('port', 0) <= 65535:
        raise ValueError('Control endpoint requires IPv4 loopback')
    token = os.environ.get(token_environment, '')
    if not token:
        raise ValueError('Control token environment is empty')
    frame = json.dumps(dict(request, schemaVersion=1, token=token)).encode() + b'\n'
    if len(frame) > 65536:
        raise ValueError('Control request exceeds 64 KiB')
    timeout = request.get('timeoutMs', 10000)
    if not isinstance(timeout, int) or not 1 <= timeout <= 60000:
        raise ValueError('timeoutMs requires 1..60000')
    with socket.create_connection((endpoint['address'], endpoint['port']), timeout=(timeout / 1000) + 3) as client:
        client.sendall(frame)
        with client.makefile('rb') as stream:
            line = stream.readline(1024 * 1024 + 1)
    if len(line) > 1024 * 1024:
        raise ValueError('Control response exceeds 1 MiB')
    result = json.loads(line)
    if result.get('schemaVersion') != 1:
        raise ValueError('Control response version mismatch')
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('command', choices=COMMANDS)
    parser.add_argument('--preset', default='msvc-release' if os.name == 'nt' else 'ninja-release')
    parser.add_argument('--target')
    parser.add_argument('--json', action='store_true')
    parser.add_argument('--endpoint', type=Path)
    parser.add_argument('--token-env', default='AZURE_CONTROL_TOKEN')
    parser.add_argument('--request', type=Path)
    parser.add_argument('--name')
    parser.add_argument('--label', default='validation')
    parser.add_argument('--script', type=Path)
    parser.add_argument('--report', type=Path)
    # The explicit separator preserves runtime option ordering and spelling.
    values = list(sys.argv[1:] if argv is None else argv)
    if '--' in values:
        split = values.index('--'); forward = values[split + 1:]; values = values[:split]
    else:
        forward = []
    args = parser.parse_args(values)
    try:
        if args.target and args.target not in TARGETS:
            raise ValueError('Unknown build target: ' + args.target)
        build, config = preset(SOURCE, args.preset)
        if args.command == 'describe' and not args.endpoint:
            data = json.loads((SOURCE / 'CMakePresets.json').read_text(encoding='utf-8'))
            print(json.dumps({'schemaVersion': 1, 'commands': COMMANDS, 'presets': [p['name'] for p in data['configurePresets']], 'targets': TARGETS}))
            return 0
        if args.command == 'doctor':
            tools = {name: shutil.which(name) for name in ('cmake', 'ctest', 'cpack', 'ninja')}
            passed = all(tools.values())
            print(json.dumps({'passed': passed, 'python': sys.executable, 'tools': tools, 'preset': args.preset,
                              'buildDirectory': str(build), 'configuration': config}))
            return 0 if passed else 1
        if args.command in ('query', 'edit', 'shot', 'capture') or (args.command == 'describe' and args.endpoint):
            if not args.endpoint:
                raise ValueError('Control operation requires --endpoint')
            request = json.loads(args.request.read_text(encoding='utf-8')) if args.request else {}
            request['op'] = 'screenshot' if args.command in ('shot', 'capture') else args.command
            if args.command == 'query':
                request['name'] = args.name
            if args.command in ('shot', 'capture'):
                request['label'] = args.label
            result = control(args.endpoint, args.token_env, request)
            print(json.dumps(result, ensure_ascii=False))
            return 0 if result.get('passed') else 1
        def invoke(command):
            if os.name == 'nt' and args.preset.startswith('msvc-'):
                command = [str(SOURCE / 'tools/msvc_env.bat'), *command]
            return execute(command, SOURCE, args.json)
        provenance = [sys.executable, str(SOURCE / 'tools/build_provenance.py')]
        if args.command == 'build':
            with build_lock(build):
                status = invoke(['cmake', '--preset', args.preset])
                if not status:
                    command = ['cmake', '--build', str(build), '--config', config, '--target', args.target] if args.target else [*provenance, 'build', '--build-dir', str(build), '--config', config]
                    status = invoke(command)
        elif args.command == 'test':
            status = invoke(['ctest', '--test-dir', str(build), '-C', config, '--output-on-failure', *forward])
        elif args.command == 'package':
            with build_lock(build):
                status = invoke(['cmake', '-DBUILD_DIR=' + str(build), '-DCONFIG=' + config, '-P', str(SOURCE / 'tools/run_release_gate.cmake')])
        elif args.command == 'provenance':
            status = invoke([*provenance, 'verify', '--build-dir', str(build), '--config', config])
        else:
            target = args.target or 'AzureRender'
            if args.command == 'validate':
                if not args.script or not args.report:
                    raise ValueError('Validation requires --script and --report')
                forward += ['--validation-script', str(args.script.resolve()), '--validation-report', str(args.report.resolve())]
            status = execute(runtime_command(build, config, target, forward), SOURCE, args.json)
        if args.json:
            print(json.dumps({'passed': status == 0, 'command': args.command, 'preset': args.preset, 'configuration': config, 'exitCode': status}))
        return status
    except (ValueError, RuntimeError, OSError, KeyError, json.JSONDecodeError) as error:
        print(json.dumps({'passed': False, 'message': str(error)}), file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
