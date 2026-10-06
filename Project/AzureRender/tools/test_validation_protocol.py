"""Exercise the production TCP transport with a minimal host."""
import argparse
import json
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor


def exchange(endpoint, token, request):
    request = dict(request, schemaVersion=1, token=token)
    with socket.create_connection((endpoint['address'], endpoint['port']), timeout=3) as client:
        client.settimeout(max(3, min(62, request.get('timeoutMs', 1000) / 1000 + 2)))
        client.sendall((json.dumps(request) + '\n').encode())
        with client.makefile('rb') as stream:
            line = stream.readline(1024 * 1024 + 1)
    assert len(line) <= 1024 * 1024
    return json.loads(line)


def main(probe):
    env = dict(os.environ, AZURE_VALIDATION_TEST_TOKEN='protocol-test-token')
    with tempfile.TemporaryDirectory(prefix='azure protocol ') as directory:
        endpoint_file = Path(directory) / 'endpoint.json'
        process = subprocess.Popen([str(probe), str(endpoint_file), '127.0.0.1'], env=env,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            deadline = time.monotonic() + 10
            while not endpoint_file.exists() and time.monotonic() < deadline:
                assert process.poll() is None, process.communicate()
                time.sleep(.02)
            endpoint = json.loads(endpoint_file.read_text())
            token = env['AZURE_VALIDATION_TEST_TOKEN']
            assert exchange(endpoint, 'invalid', dict(op='query', name='engine.status'))['code'] == 'Unauthorized'
            assert exchange(endpoint, token, dict(op='describe'))['passed']
            assert exchange(endpoint, token, dict(op='query', name='engine.status'))['value'] == 'running'
            assert not exchange(endpoint, token, dict(op='query', name='missing'))['passed']
            assert not exchange(endpoint, token, dict(op='unknown'))['passed']
            assert exchange(endpoint, token, dict(op='wait-until', name='engine.status', equals='missing', timeoutMs=1))['code'] == 'Timeout'
            assert not exchange(endpoint, token, dict(op='assert', name='engine.status', equals='missing'))['passed']
            assert exchange(endpoint, token, dict(op='wait-frames', frames=2))['passed']
            with ThreadPoolExecutor(max_workers=1) as pool:
                pending = pool.submit(exchange, endpoint, token, dict(op='wait-until', name='engine.status', equals='missing', timeoutMs=1000))
                time.sleep(.1)
                start = time.monotonic()
                assert exchange(endpoint, token, dict(op='query', name='engine.status'))['passed']
                assert time.monotonic() - start < .5, 'Pending wait blocks independent control query'
                assert pending.result()['code'] == 'Timeout'
            with socket.create_connection((endpoint['address'], endpoint['port']), timeout=3) as client:
                client.sendall(b'{' + b'x' * 65536)
                assert json.loads(client.makefile('rb').readline())['code'] == 'RequestTooLarge'
            assert exchange(endpoint, token, dict(op='edit'))['passed']
            assert process.wait(timeout=5) == 0
        finally:
            if process.poll() is None:
                process.kill()
            process.communicate()
        for address, secret in [('0.0.0.0', token), ('192.0.2.1', token), ('127.0.0.1', '')]:
            env['AZURE_VALIDATION_TEST_TOKEN'] = secret
            result = subprocess.run([str(probe), str(endpoint_file), address], env=env, capture_output=True, timeout=5)
            assert result.returncode != 0, (address, secret)
    print('Validation transport: discovery, authentication, limits, timeout and shutdown passed')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--probe', type=Path, default=Path('build/ninja-msvc-debug/AzureValidationProbe.exe'))
    main(parser.parse_args().probe.resolve())
