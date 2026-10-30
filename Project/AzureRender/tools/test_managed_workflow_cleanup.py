"""Exercise package restoration with real Windows directory leases."""
import ctypes
from ctypes import wintypes
import os
from pathlib import Path
import tempfile
import threading
import time
import unittest
from test_managed_workflows import restore_package_directory


@unittest.skipUnless(os.name == 'nt', 'Windows directory sharing contract')
class ManagedWorkflowCleanupTests(unittest.TestCase):
    def setUp(self):
        parent=Path(__file__).resolve().parents[1]/'build/runs'
        parent.mkdir(parents=True,exist_ok=True)
        self.temporary=tempfile.TemporaryDirectory(prefix='managed-restore-',dir=parent)
        self.addCleanup(self.temporary.cleanup)
        self.root=Path(self.temporary.name)
        self.source=self.root/'moved';self.source.mkdir()
        (self.source/'payload.txt').write_text('retained payload',encoding='utf-8')
        self.target=self.root/'package'
        self.kernel=ctypes.WinDLL('kernel32',use_last_error=True)
        self.kernel.CreateFileW.argtypes=[wintypes.LPCWSTR,wintypes.DWORD,wintypes.DWORD,ctypes.c_void_p,wintypes.DWORD,wintypes.DWORD,ctypes.c_void_p]
        self.kernel.CreateFileW.restype=wintypes.HANDLE
        self.kernel.CloseHandle.argtypes=[wintypes.HANDLE]
        self.kernel.CloseHandle.restype=wintypes.BOOL

    def lease(self):
        handle=self.kernel.CreateFileW(str(self.source),1,3,None,3,0x02000000,None)
        if handle==ctypes.c_void_p(-1).value:raise ctypes.WinError(ctypes.get_last_error())
        return handle

    def test_restores_after_directory_lease_is_released(self):
        handle=self.lease();released=threading.Event()
        def release():
            if not released.is_set():
                self.kernel.CloseHandle(handle);released.set()
        timer=threading.Timer(.1,release);timer.start()
        try:
            restore_package_directory(self.source,self.target,timeout=1)
            self.assertEqual((self.target/'payload.txt').read_text(encoding='utf-8'),'retained payload')
            self.assertFalse(self.source.exists())
        finally:
            timer.cancel();timer.join();release()

    def test_permanent_lease_preserves_source_and_respects_deadline(self):
        handle=self.lease();start=time.monotonic()
        try:
            with self.assertRaises(OSError):restore_package_directory(self.source,self.target,timeout=.1)
            elapsed=time.monotonic()-start
            self.assertGreaterEqual(elapsed,.09)
            self.assertLess(elapsed,1)
            self.assertTrue(self.source.exists())
            self.assertFalse(self.target.exists())
        finally:self.kernel.CloseHandle(handle)

    def test_existing_destination_is_preserved(self):
        self.target.mkdir();(self.target/'user.txt').write_text('keep',encoding='utf-8')
        with self.assertRaises(OSError):restore_package_directory(self.source,self.target,timeout=.1)
        self.assertEqual((self.target/'user.txt').read_text(encoding='utf-8'),'keep')
        self.assertTrue(self.source.exists())


if __name__=='__main__':unittest.main()
