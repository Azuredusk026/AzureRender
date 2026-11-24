"""External RenderDoc worker; keep syntax compatible with its Python 3.6 bindings."""
import json
from pathlib import Path
import subprocess
import sys
import time


def capture_target(rd, request, receipt):
    from renderdoc_capture import fingerprint
    output = Path(request['output'])
    options = rd.CaptureOptions()
    options.apiValidation = True
    options.debugOutputMute = False
    options.hookIntoChildren = False
    options.refAllResources = False
    receipt['capture_options'] = {'apiValidation': True, 'debugOutputMute': False,
        'hookIntoChildren': False, 'refAllResources': False}
    receipt['messages'] = []
    control = None
    if request['operation'] == 'inject':
        receipt['declared_pid'] = request['pid']
        receipt['source_identity_scope'] = 'caller-declared executable; observed PID is checked independently'
        result = rd.InjectIntoProcess(request['pid'], [], str(output/'capture'), options, False)
    else:
        receipt['source_identity_scope'] = 'explicit launch executable'
        result = rd.ExecuteAndInject(request['source']['path'], request['working_directory'],
            subprocess.list2cmdline(request['arguments']), [], str(output/'capture'), options, False)
    receipt['execute_result'] = str(result.result)
    receipt['ident'] = result.ident
    if result.result != rd.ResultCode.Succeeded:
        raise RuntimeError('RenderDoc capture initialization failed: ' + str(result.result))
    try:
        deadline = time.monotonic() + min(15, request['timeout'])
        while time.monotonic() < deadline:
            # The returned ident is the only candidate. Do not scan other targets.
            control = rd.CreateTargetControl('', result.ident, 'AzureRender evidence', False)
            if control:
                break
            time.sleep(.05)
        if not control:
            raise RuntimeError('Target control unavailable at returned ident')
        receipt['pid'] = control.GetPID()
        receipt['target'] = control.GetTarget()
        receipt['api_at_connection'] = control.GetAPI()
        if request['operation'] == 'inject':
            if control.GetPID() != request['pid']:
                raise RuntimeError('Observed process does not match the selected PID')
            control.TriggerCapture(1)
        else:
            control.QueueCapture(request['frame'], 1)
        deadline = time.monotonic() + request['timeout']
        while time.monotonic() < deadline and control.Connected():
            message = control.ReceiveMessage(None)
            if message.type != rd.TargetControlMessageType.Noop:
                receipt['messages'].append({'type': str(message.type), 'at_unix': time.time()})
            if message.type == rd.TargetControlMessageType.NewCapture:
                data = message.newCapture
                receipt['capture'] = fingerprint(data.path)
                receipt['artifacts'].append(receipt['capture'])
                receipt['capture_frame'] = data.frameNumber
                receipt['api'] = str(data.api)
                receipt['status'] = 'captured'
                return
            if message.type == rd.TargetControlMessageType.Disconnected:
                break
        raise RuntimeError('Target returned no capture within the budget')
    finally:
        if control:
            control.Shutdown()


def main(request_path):
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from renderdoc_capture import fingerprint
    request = json.loads(Path(request_path).read_text(encoding='utf8'))
    output = Path(request['output'])
    receipt = {'format': 'RenderDocEvidence/1', 'operation': request['operation'],
        'source': request['source'], 'status': 'failed', 'artifacts': [],
        'started_unix': time.time(), 'request': request}
    initialized = False
    result = 1
    try:
        if fingerprint(request['source']['path']) != request['source']:
            raise ValueError('Source changed before the worker started')
        sys.path.insert(0, request['renderdoc_dir'])
        import renderdoc as rd
        receipt['tool_version'] = rd.GetVersionString()
        rd.InitialiseReplay(rd.GlobalEnvironment(), [])
        initialized = True
        if request['operation'] == 'replay':
            from renderdoc_replay import replay_capture
            replay_capture(rd, request, receipt)
        else:
            capture_target(rd, request, receipt)
        if fingerprint(request['source']['path']) != request['source']:
            raise ValueError('Source changed during the RenderDoc operation')
        result = 0
    except Exception as error:
        receipt['status'] = 'failed'
        receipt['error'] = repr(error)
        print(repr(error))
    finally:
        if initialized:
            rd.ShutdownReplay()
        receipt['finished_unix'] = time.time()
        (output/'receipt.json').write_text(json.dumps(receipt, indent=2), encoding='utf8')
    return result


if __name__ == '__main__':
    sys.exit(main(sys.argv[1]))
