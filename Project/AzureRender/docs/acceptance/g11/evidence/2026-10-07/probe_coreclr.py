"""Test the installed hostfxr with a real unmanaged managed entry point."""
import ctypes
import json
from pathlib import Path
import time

work = Path(__file__).resolve().parent
probe = work / 'toolchain-probe'
assembly = probe / 'bin/Release/net9.0/win-x64/Probe.dll'
assert assembly.is_file()
config = probe / 'Probe.runtimeconfig.json'
config.write_text(json.dumps({'runtimeOptions': {'tfm': 'net9.0', 'framework': {
    'name': 'Microsoft.NETCore.App', 'version': '9.0.11'}}}))
started = time.perf_counter()
host = ctypes.WinDLL('C:/Program Files/dotnet/host/fxr/9.0.11/hostfxr.dll')
initialize = host.hostfxr_initialize_for_runtime_config
initialize.argtypes = [ctypes.c_wchar_p, ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p)]
initialize.restype = ctypes.c_int32
handle = ctypes.c_void_p()
assert initialize(str(config), None, ctypes.byref(handle)) >= 0
get_delegate = host.hostfxr_get_runtime_delegate
get_delegate.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.POINTER(ctypes.c_void_p)]
get_delegate.restype = ctypes.c_int32
entry = ctypes.c_void_p()
assert get_delegate(handle, 5, ctypes.byref(entry)) >= 0
load = ctypes.WINFUNCTYPE(ctypes.c_int32, ctypes.c_wchar_p, ctypes.c_wchar_p,
    ctypes.c_wchar_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p))(entry.value)
function = ctypes.c_void_p()
assert load(str(assembly), 'Probe, Probe', 'Invoke', ctypes.c_void_p(-1),
    None, ctypes.byref(function)) >= 0
invoke = ctypes.CFUNCTYPE(ctypes.c_int32, ctypes.c_int32)(function.value)
assert invoke(41) == 42
startup_ms = (time.perf_counter() - started) * 1000
host.hostfxr_close.argtypes = [ctypes.c_void_p]
host.hostfxr_close.restype = ctypes.c_int32
assert host.hostfxr_close(handle) == 0
result = {'status': 'passed', 'backend': 'CoreCLR', 'runtime': '9.0.11',
    'input': 41, 'output': 42, 'startupMs': startup_ms,
    'scope': 'toolchain probe; not script lifecycle acceptance'}
(work / 'coreclr-toolchain-result.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result))
