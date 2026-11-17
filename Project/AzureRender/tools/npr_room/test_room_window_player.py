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
from test_story_signal_room_player import entry
from tools.npr_room.build_story_signal_room import GRAPH
from tools.npr_room.room_language import FIELDS
from tools.npr_room.window_settings_room import bind_display_language, display_localization, install


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


TOPMOST_MENU_COMMAND = 8191


def native_menu_title(window):
    hwnd = window.find()
    assert hwnd
    user = window.user
    user.GetSystemMenu.argtypes = [wintypes.HWND, wintypes.BOOL]
    user.GetSystemMenu.restype = wintypes.HMENU
    user.GetMenuItemID.argtypes = [wintypes.HMENU, ctypes.c_int]
    user.GetMenuItemID.restype = wintypes.UINT
    user.GetMenuStringW.argtypes = [wintypes.HMENU, wintypes.UINT, wintypes.LPWSTR, ctypes.c_int, wintypes.UINT]
    user.GetMenuStringW.restype = ctypes.c_int
    menu = user.GetSystemMenu(hwnd, False)
    assert menu and user.GetMenuItemID(menu, 0) == TOPMOST_MENU_COMMAND, "Missing topmost system menu entry"
    title = ctypes.create_unicode_buffer(128)
    assert user.GetMenuStringW(menu, TOPMOST_MENU_COMMAND, title, len(title), 0)
    return title.value


def activate_native_topmost(window):
    hwnd = window.find()
    assert hwnd
    send = window.user.SendMessageTimeoutW
    send.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM, wintypes.UINT,
                     wintypes.UINT, ctypes.POINTER(ctypes.c_size_t)]
    send.restype = wintypes.LPARAM
    result = ctypes.c_size_t()
    assert send(hwnd, 0x0112, TOPMOST_MENU_COMMAND, 0, 2, 2000, ctypes.byref(result))


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


def settings_panel(player, project, resources, output, system_menu=False, inventory=None):
    output.mkdir()
    profile = output / "profile"
    room = project.parent
    root = room / "assets"
    document = (root / "timer.rml").read_text(encoding="utf-8")
    graph = json.loads((root / GRAPH).read_bytes())
    timer = (root / "pomodoro.lua").read_text(encoding="utf-8")
    localization = None
    translated = {}
    if inventory:
        localization = display_localization(room, inventory)
        timer = bind_display_language(timer)
        translated = {row["ID"]: row for row in localization if row["ID"] in (
            "ui_setting_general_alwaysontop_title", "ui_global_on", "ui_global_off")}
    prepared = {}
    locale = "ZhHans" if inventory else "En"
    locale_titles = {}

    def ready(window, _):
        prepared["window"] = window

    def state():
        return native_placement(prepared["window"])

    def caption(topmost):
        if not translated:
            return "Always on Top(Off)" if topmost else "Always on Top(On)"
        title = translated["ui_setting_general_alwaysontop_title"][locale]
        verb = translated["ui_global_off" if topmost else "ui_global_on"][locale]
        return title + "(" + verb + ")"

    def menu_toggle(on):
        window = prepared["window"]
        assert native_menu_title(window) == caption(not on)
        activate_native_topmost(window)
        saved = wait_for_slot(profile, "room-window-settings", lambda value: value == [1, 0, 3, on], 10)
        deadline = time.monotonic() + 10
        expected_title = caption(on)
        while state()["topmost"] != on or native_menu_title(window) != expected_title:
            assert time.monotonic() < deadline, (state(), native_menu_title(window))
            time.sleep(.03)
        return {"saved": saved, "nativeTopmost": on, "caption": expected_title}

    def configure(descriptor, level, prefix):
        return install(descriptor, level, document, prefix, localization)

    with entry(room, graph, timer, additional_room=configure) as (fixture, level):
        with playing(player, fixture, resources, profile, output / "edit", level_reference=level,
                     on_ready=ready) as (click, _, _, screenshot):
            assert state()["clientPixels"] == [1280, 720]
            if inventory:
                window = prepared["window"]
                assert native_menu_title(window) == caption(False)
                locale_titles[locale] = caption(False)
                click("room-language-toggle")
                for index, selection in enumerate(FIELDS, 1):
                    if selection == "ZhHans":
                        continue
                    click(f"room-language-{index}")
                    locale = selection
                    assert slot(profile, "room-language") == [1, index]
                    observed = native_menu_title(window)
                    assert observed == caption(False), (locale, observed, caption(False))
                    locale_titles[locale] = observed
                click("room-language-toggle")
            menu = [menu_toggle(True), menu_toggle(False)] if system_menu else []
            click("display-toggle")
            click("display-resolution")
            click("display-resolution")
            preset = state()
            assert preset["clientPixels"] == [1600, 900] and preset["decorated"], preset
            click("display-topmost")
            assert state()["topmost"]
            panel = screenshot("display-settings.png")
            with Image.open(panel) as image:
                assert image.size == (1600, 900)
            click("display-mode")
            borderless = state()
            assert borderless["window"] == borderless["monitor"] and not borderless["decorated"] and borderless["topmost"]
            click("display-mode")
            restored = state()
            assert restored["clientPixels"] == [1600, 900] and restored["decorated"] and restored["topmost"]
            assert prepared["window"].resize(1100, 700)
            custom = wait_for_slot(profile, "room-window-settings", lambda value: value == [1, 0, 3, True], 10)
            click("display-mode")
            selected = wait_for_slot(profile, "room-window-settings", lambda value: value == [1, 1, 3, True], 10)
        with playing(player, fixture, resources, profile, output / "reload", level_reference=level,
                     on_ready=ready) as (click, _, _, _):
            reopened = state()
            assert reopened["window"] == reopened["monitor"] and not reopened["decorated"] and reopened["topmost"]
            assert slot(profile, "room-window-settings") == selected
            if inventory:
                assert slot(profile, "room-language") == [1, FIELDS.index(locale) + 1]
                assert native_menu_title(prepared["window"]) == caption(True)
            click("display-toggle")
            click("display-topmost")
            assert not state()["topmost"]
            assert slot(profile, "room-window-settings") == [1, 1, 3, False]
    for folder in ("edit", "reload"):
        assert "Allocator after unload: buffers=0 images=0" in (output / folder / "stdout.log").read_text(encoding="utf-8")
    report = {"preset": preset, "borderless": borderless, "restored": restored, "reopened": reopened,
              "savedCustom": custom, "savedBorderless": selected, "finalTopmost": False, "cleanAllocatorExits": 2,
              "panelSHA256": hashlib.sha256(panel.read_bytes()).hexdigest()}
    if system_menu:
        report["systemMenu"] = menu
    if inventory:
        report["localeTitles"] = locale_titles
        report["reopenedLocale"] = locale
    (output / "window-settings-panel-replay.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
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


def run_in_physical_pixels(player, project, resources, output, preferences=False, panel=False, system_menu=False,
                           display_inventory=None):
    user = ctypes.WinDLL("user32", use_last_error=True)
    set_context = user.SetThreadDpiAwarenessContext
    set_context.argtypes = [ctypes.c_void_p]
    set_context.restype = ctypes.c_void_p
    # PER_MONITOR_AWARE_V2 avoids DPI virtualization of HWND coordinates.
    previous = set_context(-4)
    if not previous:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        if panel or system_menu:
            return settings_panel(player, project, resources, output, system_menu, display_inventory)
        verify = startup_preferences if preferences else replay
        return verify(player, project, resources, output)
    finally:
        assert set_context(previous)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("player", "project", "resource-root", "output"):
        parser.add_argument("--" + argument, type=Path, required=True)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--preferences", action="store_true")
    mode.add_argument("--settings-panel", action="store_true")
    mode.add_argument("--system-menu", action="store_true")
    parser.add_argument("--display-inventory", type=Path)
    args = parser.parse_args()
    if args.display_inventory and not (args.settings_panel or args.system_menu):
        parser.error("--display-inventory requires a display panel or system menu replay")
    print(json.dumps(run_in_physical_pixels(args.player.resolve(), args.project.resolve(),
                                           args.resource_root.resolve(), args.output.resolve(), args.preferences,
                                           args.settings_panel, args.system_menu, args.display_inventory)))
