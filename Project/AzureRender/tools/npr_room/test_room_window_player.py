"""Exercise the normal room through native Windows resize, minimize, and restart."""

import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
from pathlib import Path
import time

from PIL import Image

from test_notes_player import playing
from test_story_player import slot, wait_for_slot


def native_placement(window):
    class MonitorInfo(ctypes.Structure):
        _fields_ = [("size", wintypes.DWORD), ("monitor", wintypes.RECT),
                    ("work", wintypes.RECT), ("flags", wintypes.DWORD)]

    hwnd = window.find()
    assert hwnd
    user = window.user
    user.MonitorFromWindow.argtypes = [wintypes.HWND, wintypes.DWORD]
    user.MonitorFromWindow.restype = wintypes.HMONITOR
    user.GetMonitorInfoW.argtypes = [wintypes.HMONITOR, ctypes.POINTER(MonitorInfo)]
    user.GetWindowLongPtrW.argtypes = [wintypes.HWND, ctypes.c_int]
    user.GetWindowLongPtrW.restype = ctypes.c_ssize_t
    info = MonitorInfo()
    info.size = ctypes.sizeof(info)
    assert user.GetMonitorInfoW(user.MonitorFromWindow(hwnd, 2), ctypes.byref(info))
    bounds = wintypes.RECT()
    assert user.GetWindowRect(hwnd, ctypes.byref(bounds))
    rect = lambda value: [value.left, value.top, value.right, value.bottom]
    return {"clientPixels": window.extent(), "window": rect(bounds), "monitor": rect(info.monitor),
            "decorated": bool(user.GetWindowLongPtrW(hwnd, -16) & 0xc00000),
            "topmost": bool(user.GetWindowLongPtrW(hwnd, -20) & 8)}


def startup_preferences(player, project, resources, output):
    output.mkdir()
    profile = output / "profile"
    settings = output / "window-settings.json"
    settings.write_text(json.dumps({"schemaVersion": 1, "values": {
        "runtime.windowMode": 1, "runtime.windowResolution": 2, "runtime.alwaysOnTop": True}}), encoding="utf-8")
    saved = settings.read_bytes()
    results = {}
    for label, overrides in (
        ("borderless", ()),
        ("windowed", ("--set", "runtime.windowMode=0", "--set", "runtime.windowResolution=1",
                      "--set", "runtime.alwaysOnTop=false")),
        ("reopened-borderless", ()),
    ):
        folder = output / label
        placement = {}

        def ready(window, _):
            placement.update(native_placement(window))

        with playing(player, project, resources, profile, folder, on_ready=ready,
                     player_args=("--settings-file", str(settings), *overrides)) as (_, _, _, screenshot):
            if label == "windowed":
                assert placement["decorated"] and not placement["topmost"], placement
                assert placement["clientPixels"] == [1600, 900], placement
            else:
                assert not placement["decorated"] and placement["topmost"], placement
                assert placement["window"] == placement["monitor"], placement
                left, top, right, bottom = placement["monitor"]
                assert placement["clientPixels"] == [right - left, bottom - top], placement
            if label != "reopened-borderless":
                path = screenshot(label + ".png")
                with Image.open(path) as image:
                    assert list(image.size) == placement["clientPixels"]
                    assert any(low < high for low, high in image.convert("RGB").getextrema())
                placement["imageSHA256"] = hashlib.sha256(path.read_bytes()).hexdigest()
        assert "Allocator after unload: buffers=0 images=0" in (folder / "stdout.log").read_text(encoding="utf-8")
        results[label] = placement
    assert settings.read_bytes() == saved
    report = {"windows": results, "independentPlayers": 3, "cleanAllocatorExits": 3}
    (output / "window-preferences-replay.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return report


def replay(player, project, resources, output):
    output.mkdir()
    profile = output / "profile"
    captures = []
    client = {}

    def on_ready(window, call):
        assert window.find()
        client.update(window=window, call=call)

    def state():
        observed = json.loads(client["call"]("query", name="game.state"))
        # Periodic asset refresh also sets loading while the committed room renders.
        assert observed["levelReference"] == "assets:/room.azurelevel", observed
        assert not any(observed[key] for key in ("scriptErrors", "presentationErrors", "loadError")), observed
        return observed

    def capture(screenshot, name, size, before):
        window = client["window"]
        assert window.extent() == list(size), window.extent()
        client["call"]("wait-frames", frames=12)
        after = state()
        assert after["frame"] > before["frame"], (before, after)
        assert after["levelRevision"] == before["levelRevision"], (before, after)
        path = screenshot(name + ".png")
        with Image.open(path) as image:
            assert image.size == size, (name, image.size, size)
            assert any(low < high for low, high in image.convert("RGB").getextrema()), name
        captures.append({"stage": name, "clientPixels": list(size), "renderPixels": list(size),
                         "frame": after["frame"], "levelRevision": after["levelRevision"],
                         "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
        return after

    with playing(player, project, resources, profile, output / "window", on_ready=on_ready) as (
        click, _, _, screenshot,
    ):
        window = client["window"]
        before = capture(screenshot, "boot", (1280, 720), state())
        assert window.resize(960, 540)
        before = capture(screenshot, "compact", (960, 540), before)
        click("timer-mode-button")
        click("timer-toggle-button")
        assert window.minimize(), "The Player HWND did not become iconic"
        time.sleep(.25)
        assert window.restore(), "The Player HWND did not leave its iconic state"
        before = capture(screenshot, "restored", (960, 540), before)
        assert window.resize(1920, 1080)
        capture(screenshot, "full-hd", (1920, 1080), before)
        time.sleep(1.25)
        click("timer-end-button")
        assert slot(profile, "timer-mode") == [1, 1]
        recorded = wait_for_slot(profile, "countup-session",
                                 lambda value: value[:2] == [1, 0] and value[2] >= 1, 12)

    with playing(player, project, resources, profile, output / "restart", on_ready=on_ready) as (
        click, _, _, screenshot,
    ):
        assert slot(profile, "timer-mode") == [1, 1]
        assert slot(profile, "countup-session") == recorded
        click("notes-toggle")
        capture(screenshot, "restarted", (1280, 720), state())

    report = {"independentPlayers": 2, "minimized": True, "restored": True, "captures": captures,
              "persistedTimerMode": [1, 1], "persistedCountupSession": recorded, "cleanAllocatorExits": 2}
    (output / "window-replay.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return report


def run_in_physical_pixels(player, project, resources, output, preferences=False):
    user = ctypes.WinDLL("user32", use_last_error=True)
    set_context = user.SetThreadDpiAwarenessContext
    set_context.argtypes = [ctypes.c_void_p]
    set_context.restype = ctypes.c_void_p
    # PER_MONITOR_AWARE_V2 avoids DPI virtualization of HWND coordinates.
    previous = set_context(-4)
    if not previous:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        return (startup_preferences if preferences else replay)(player, project, resources, output)
    finally:
        assert set_context(previous)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("player", "project", "resource-root", "output"):
        parser.add_argument("--" + argument, type=Path, required=True)
    parser.add_argument("--preferences", action="store_true")
    args = parser.parse_args()
    print(json.dumps(run_in_physical_pixels(args.player.resolve(), args.project.resolve(),
                                           args.resource_root.resolve(), args.output.resolve(), args.preferences)))
