import ctypes
from ctypes import wintypes
import json
user=ctypes.WinDLL('user32')
callback=ctypes.WINFUNCTYPE(wintypes.BOOL,wintypes.HWND,wintypes.LPARAM)
user.EnumWindows.argtypes=[callback,wintypes.LPARAM]
user.GetWindowThreadProcessId.argtypes=[wintypes.HWND,ctypes.POINTER(wintypes.DWORD)]
user.GetWindowTextW.argtypes=[wintypes.HWND,wintypes.LPWSTR,ctypes.c_int]
user.IsWindowVisible.argtypes=[wintypes.HWND]
user.IsIconic.argtypes=[wintypes.HWND]
rows=[]
def visit(window,_):
    process=wintypes.DWORD()
    user.GetWindowThreadProcessId(window,ctypes.byref(process))
    if process.value==14916 and user.IsWindowVisible(window):
        title=ctypes.create_unicode_buffer(512)
        user.GetWindowTextW(window,title,512)
        rows.append(dict(handle=int(window),pid=process.value,title=title.value,minimized=bool(user.IsIconic(window))))
    return True
user.EnumWindows(callback(visit),0)
print(json.dumps(rows,ensure_ascii=False))
