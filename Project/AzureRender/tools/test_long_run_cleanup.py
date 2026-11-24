"""Exercise cleanup against real responsive and stalled window processes."""
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
import run_playable_long_run as runner


def window_fixture(mode):
    user = ctypes.WinDLL('user32', use_last_error=True)
    user.CreateWindowExW.argtypes = [wintypes.DWORD, wintypes.LPCWSTR,
        wintypes.LPCWSTR, wintypes.DWORD, ctypes.c_int, ctypes.c_int,
        ctypes.c_int, ctypes.c_int, wintypes.HWND, wintypes.HMENU,
        wintypes.HINSTANCE, ctypes.c_void_p]
    user.CreateWindowExW.restype = wintypes.HWND
    user.PeekMessageW.argtypes = [ctypes.POINTER(wintypes.MSG),
        wintypes.HWND, wintypes.UINT, wintypes.UINT, wintypes.UINT]
    user.DispatchMessageW.argtypes = [ctypes.POINTER(wintypes.MSG)]
    user.IsWindow.argtypes = [wintypes.HWND]
    handle = user.CreateWindowExW(0, 'STATIC', 'AzureCleanupFixture',
        0x00CF0000, 0, 0, 320, 240, None, None, None, None)
    if not handle:
        raise ctypes.WinError(ctypes.get_last_error())
    print(handle, flush=True)
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        if mode == 'responsive':
            message = wintypes.MSG()
            while user.PeekMessageW(ctypes.byref(message), None, 0, 0, 1):
                user.DispatchMessageW(ctypes.byref(message))
            if not user.IsWindow(handle):
                return
        time.sleep(.01)


@unittest.skipUnless(os.name == 'nt', 'Native Windows window fixture')
class CleanupTests(unittest.TestCase):
    def finalize(self):
        function = getattr(runner, 'finalize_run', None)
        self.assertTrue(callable(function), 'A bounded failure cleanup entry is required')
        return function

    def exercise(self, mode, *, invalid_handle=False, scenario=None):
        function = self.finalize()
        process = subprocess.Popen([sys.executable, str(Path(__file__).resolve()),
            '--fixture', mode], stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            text=True)
        try:
            window = runner.NativeWindow(process.pid, 'AzureCleanupFixture')
            window.handle = int(process.stdout.readline())
            if invalid_handle:
                window.handle = 0x12345678
            with tempfile.TemporaryDirectory() as folder:
                output = Path(folder)
                observations = [{'seconds': 1.5, 'working': 12345}]
                started = time.monotonic()
                function(process, window, output, observations, scenario,
                    grace_seconds=1)
                elapsed = time.monotonic() - started
                self.assertLess(elapsed, 3, 'Cleanup waited on the stalled window thread')
                self.assertIsNotNone(process.poll(), 'Owned process was left running')
                self.assertEqual(json.loads((output/'observation.json').read_text()), observations)
                return json.loads((output/'cleanup.json').read_text())
        finally:
            if process.poll() is None:
                process.kill()
            process.wait(timeout=5)
            process.stdout.close()
            process.stderr.close()

    def test_stalled_window_exits_with_forced_cleanup_and_keeps_observations(self):
        result = self.exercise('stalled')
        self.assertTrue(result['forcedTermination'])
        self.assertFalse(result['normalExit'])

    def test_responsive_window_closes_normally(self):
        result = self.exercise('responsive')
        self.assertFalse(result['forcedTermination'])
        self.assertTrue(result['normalExit'])
        self.assertEqual(result['errors'], [])

    def test_failed_close_request_still_reaps_owned_process(self):
        result = self.exercise('stalled', invalid_handle=True)
        self.assertTrue(result['forcedTermination'])
        self.assertTrue(result['errors'])

    def test_scenario_cleanup_failure_is_recorded_after_process_exit(self):
        class BrokenScenario:
            def close(self):
                raise RuntimeError('fixture scenario close failure')
        result = self.exercise('responsive', scenario=BrokenScenario())
        self.assertTrue(result['normalExit'])
        self.assertIn('fixture scenario close failure', result['errors'][0])

    def test_missing_window_terminates_owned_process(self):
        function = self.finalize()
        process = subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(5)'])
        try:
            with tempfile.TemporaryDirectory() as folder:
                window = runner.NativeWindow(process.pid)
                function(process, window, Path(folder), [], grace_seconds=.2)
                result = json.loads((Path(folder)/'cleanup.json').read_text())
                self.assertTrue(result['forcedTermination'])
                self.assertIsNotNone(process.poll())
        finally:
            if process.poll() is None:
                process.kill()
            process.wait(timeout=5)

    def test_already_exited_process_keeps_return_code_and_observations(self):
        function = self.finalize()
        process = subprocess.Popen([sys.executable, '-c', 'raise SystemExit(7)'])
        process.wait(timeout=5)
        with tempfile.TemporaryDirectory() as folder:
            function(process, runner.NativeWindow(process.pid), Path(folder), [])
            result = json.loads((Path(folder)/'cleanup.json').read_text())
            self.assertEqual(result['exitCode'], 7)
            self.assertFalse(result['normalExit'])
            self.assertFalse(result['forcedTermination'])


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == '--fixture':
        window_fixture(sys.argv[2])
    else:
        unittest.main()
