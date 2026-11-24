"""Find the engine window when a driver creates another large owned window."""
import ctypes
from ctypes import wintypes
import os
import unittest
from run_playable_long_run import NativeWindow


@unittest.skipUnless(os.name == 'nt', 'Windows window ownership contract')
class NativeWindowTests(unittest.TestCase):
    def test_driver_window_cannot_receive_engine_close(self):
        user = ctypes.WinDLL('user32', use_last_error=True)
        user.CreateWindowExW.argtypes = [wintypes.DWORD, wintypes.LPCWSTR, wintypes.LPCWSTR,
            wintypes.DWORD, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int,
            wintypes.HWND, wintypes.HMENU, wintypes.HINSTANCE, ctypes.c_void_p]
        user.CreateWindowExW.restype = wintypes.HWND
        user.DestroyWindow.argtypes = [wintypes.HWND]
        windows = []
        try:
            for title in ('AzureRender Editor Preview', '__wglDummyWindowFodder'):
                handle = user.CreateWindowExW(0, 'STATIC', title, 0x00CF0000,
                    0, 0, 800, 600, None, None, None, None)
                self.assertTrue(handle, ctypes.get_last_error())
                windows.append(handle)
            engine = NativeWindow(os.getpid())
            self.assertEqual(engine.find(), windows[0],
                'The native close target must be the engine window, regardless of driver helper size')
            self.assertEqual(NativeWindow(os.getpid(), '__wglDummyWindowFodder').find(), windows[1],
                'Independent tools may declare their own window title prefix')
            user.DestroyWindow(windows.pop(0))
            self.assertIsNone(engine.find(), 'A destroyed engine window must retire its cached identity')
        finally:
            for handle in reversed(windows):
                user.DestroyWindow(handle)


if __name__ == '__main__':
    unittest.main()
