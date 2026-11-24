"""Keep each managed Windows command and its descendants in one job object."""
import ctypes
from ctypes import wintypes
import os
from pathlib import Path
import subprocess
import sys


class BasicLimits(ctypes.Structure):
    _fields_ = [
        ('processTime', ctypes.c_longlong),
        ('jobTime', ctypes.c_longlong),
        ('flags', wintypes.DWORD),
        ('minimumWorkingSet', ctypes.c_size_t),
        ('maximumWorkingSet', ctypes.c_size_t),
        ('activeProcesses', wintypes.DWORD),
        ('affinity', ctypes.c_size_t),
        ('priorityClass', wintypes.DWORD),
        ('schedulingClass', wintypes.DWORD),
    ]


class ExtendedLimits(ctypes.Structure):
    _fields_ = [
        ('basic', BasicLimits),
        ('ioCounters', ctypes.c_ulonglong * 6),
        ('processMemory', ctypes.c_size_t),
        ('jobMemory', ctypes.c_size_t),
        ('peakProcessMemory', ctypes.c_size_t),
        ('peakJobMemory', ctypes.c_size_t),
    ]


kernel = ctypes.WinDLL('kernel32', use_last_error=True)
kernel.CreateJobObjectW.argtypes = [ctypes.c_void_p, wintypes.LPCWSTR]
kernel.CreateJobObjectW.restype = wintypes.HANDLE
kernel.SetInformationJobObject.argtypes = [wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD]
kernel.SetInformationJobObject.restype = wintypes.BOOL
kernel.AssignProcessToJobObject.argtypes = [wintypes.HANDLE, wintypes.HANDLE]
kernel.AssignProcessToJobObject.restype = wintypes.BOOL
kernel.GetCurrentProcess.argtypes = []
kernel.GetCurrentProcess.restype = wintypes.HANDLE
kernel.TerminateJobObject.argtypes = [wintypes.HANDLE, wintypes.UINT]
kernel.TerminateJobObject.restype = wintypes.BOOL
kernel.CloseHandle.argtypes = [wintypes.HANDLE]
kernel.CloseHandle.restype = wintypes.BOOL
kernel.ExitProcess.argtypes = [wintypes.UINT]
kernel.ExitProcess.restype = None

EXTENDED_LIMIT_INFORMATION = 9
KILL_ON_JOB_CLOSE = 0x00002000


def checked(value):
    if not value:
        raise ctypes.WinError(ctypes.get_last_error())
    return value


class WindowsJob:
    def __init__(self):
        self.handle = checked(kernel.CreateJobObjectW(None, None))
        limits = ExtendedLimits()
        limits.basic.flags = KILL_ON_JOB_CLOSE
        try:
            checked(kernel.SetInformationJobObject(
                self.handle, EXTENDED_LIMIT_INFORMATION, ctypes.byref(limits), ctypes.sizeof(limits)))
        except OSError:
            self.close()
            raise

    def start(self, command, **kwargs):
        startup = subprocess.STARTUPINFO(lpAttributeList={'handle_list': [self.handle]})
        launcher = [sys.executable, str(Path(__file__).resolve()), str(self.handle)]
        os.set_handle_inheritable(self.handle, True)
        try:
            return subprocess.Popen([*launcher, *command], startupinfo=startup, **kwargs)
        finally:
            os.set_handle_inheritable(self.handle, False)

    def terminate(self, process):
        checked(kernel.TerminateJobObject(self.handle, 1))
        if process.poll() is None:
            process.kill()

    def close(self):
        if self.handle is not None:
            checked(kernel.CloseHandle(self.handle))
            self.handle = None


if __name__ == '__main__':
    borrowed_handle = int(sys.argv[1])
    try:
        # Attach before starting the command so every descendant inherits membership.
        checked(kernel.AssignProcessToJobObject(borrowed_handle, kernel.GetCurrentProcess()))
    finally:
        checked(kernel.CloseHandle(borrowed_handle))
    kernel.ExitProcess(subprocess.call(sys.argv[2:]))
