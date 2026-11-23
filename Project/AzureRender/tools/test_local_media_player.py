"""Replay native media selection, production playback, image rendering and recovery."""
import argparse
import ctypes
from ctypes import wintypes as wt
import json
import os
from pathlib import Path
import shutil
import struct
import sys
import threading
import time
import zlib

from build_local_media_example import build
sys.path.insert(0, str(Path(__file__).resolve().parent / "npr_room"))
from test_notes_player import playing
from test_original_music_player import digest, write


def snapshot_bytes(path):
    """Read a complete save using the production shared-slot lock protocol."""
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    class Overlapped(ctypes.Structure):
        _fields_ = [("internal", ctypes.c_size_t), ("internal_high", ctypes.c_size_t),
                    ("offset", wt.DWORD), ("offset_high", wt.DWORD), ("event", wt.HANDLE)]
    kernel.CreateFileW.argtypes = [wt.LPCWSTR, wt.DWORD, wt.DWORD, ctypes.c_void_p,
                                  wt.DWORD, wt.DWORD, wt.HANDLE]
    kernel.CreateFileW.restype = wt.HANDLE
    kernel.CloseHandle.argtypes = [wt.HANDLE]
    kernel.LockFileEx.argtypes = [wt.HANDLE, wt.DWORD, wt.DWORD, wt.DWORD, wt.DWORD, ctypes.POINTER(Overlapped)]
    kernel.UnlockFileEx.argtypes = [wt.HANDLE, wt.DWORD, wt.DWORD, wt.DWORD, ctypes.POINTER(Overlapped)]
    kernel.GetFileSizeEx.argtypes = [wt.HANDLE, ctypes.POINTER(ctypes.c_longlong)]
    kernel.ReadFile.argtypes = [wt.HANDLE, ctypes.c_void_p, wt.DWORD, ctypes.POINTER(wt.DWORD), ctypes.c_void_p]
    invalid = ctypes.c_void_p(-1).value
    lock = kernel.CreateFileW(str(path.with_name("." + path.stem + ".lock")), 0x80000000, 3, None, 3, 0, None)
    assert lock != invalid, ctypes.get_last_error()
    overlap, held = Overlapped(), False
    try:
        deadline = time.monotonic() + 5
        while not kernel.LockFileEx(lock, 1, 0, 1, 0, ctypes.byref(overlap)):
            assert ctypes.get_last_error() == 33 and time.monotonic() < deadline, ctypes.get_last_error()
            time.sleep(.002)
        held = True
        file = kernel.CreateFileW(str(path), 0x80000000, 7, None, 3, 0, None)
        assert file != invalid, ctypes.get_last_error()
        try:
            size = ctypes.c_longlong()
            assert kernel.GetFileSizeEx(file, ctypes.byref(size)) and 0 < size.value <= 1024 * 1024
            data, read = ctypes.create_string_buffer(size.value), wt.DWORD()
            assert kernel.ReadFile(file, data, size.value, ctypes.byref(read), None) and read.value == size.value
            return data.raw
        finally:
            kernel.CloseHandle(file)
    finally:
        if held: kernel.UnlockFileEx(lock, 0, 1, 0, ctypes.byref(overlap))
        kernel.CloseHandle(lock)


def native_selection(pid, title, names, click, directory=False):
    errors = []
    def drive():
        dialog = None
        try:
            user = ctypes.WinDLL("user32", use_last_error=True)
            callback = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
            user.EnumWindows.argtypes = [callback, wt.LPARAM]
            user.EnumChildWindows.argtypes = [wt.HWND, callback, wt.LPARAM]
            user.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]
            user.GetWindowTextW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
            user.GetClassNameW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
            user.GetWindowRect.argtypes = [wt.HWND, ctypes.POINTER(wt.RECT)]
            user.IsWindowEnabled.argtypes = [wt.HWND]
            user.IsWindowVisible.argtypes = [wt.HWND]
            user.GetDlgCtrlID.argtypes = [wt.HWND]
            user.IsWindow.argtypes = [wt.HWND]
            user.GetDlgItem.argtypes = [wt.HWND, ctypes.c_int]
            user.GetDlgItem.restype = wt.HWND
            user.SendMessageTimeoutW.argtypes = [wt.HWND, wt.UINT, wt.WPARAM, wt.LPARAM, wt.UINT,
                                               wt.UINT, ctypes.POINTER(ctypes.c_size_t)]
            user.PostMessageW.argtypes = [wt.HWND, wt.UINT, wt.WPARAM, wt.LPARAM]
            def visit(handle, _):
                nonlocal dialog
                owner = wt.DWORD(); user.GetWindowThreadProcessId(handle, ctypes.byref(owner))
                text = ctypes.create_unicode_buffer(256); user.GetWindowTextW(handle, text, 256)
                if owner.value == pid and text.value == title:
                    dialog = handle; return False
                return True
            deadline = time.monotonic() + 15
            while not dialog and time.monotonic() < deadline:
                user.EnumWindows(callback(visit), 0); time.sleep(.03)
            assert dialog, (pid, title, "native dialog did not open")
            def send(handle, message, w=0, l=0):
                result = ctypes.c_size_t()
                assert user.SendMessageTimeoutW(handle, message, w, l, 2, 2500, ctypes.byref(result)), (
                    message, ctypes.get_last_error())
            if names is not None:
                fields = []
                def child(handle, _):
                    kind = ctypes.create_unicode_buffer(64); user.GetClassNameW(handle, kind, 64)
                    rect = wt.RECT(); user.GetWindowRect(handle, ctypes.byref(rect))
                    if kind.value == "Edit" and user.IsWindowEnabled(handle) and user.IsWindowVisible(handle) \
                            and rect.bottom > rect.top and rect.right > rect.left:
                        fields.append((rect.top, handle))
                    return True
                while not fields and time.monotonic() < deadline:
                    user.EnumChildWindows(dialog, callback(child), 0)
                    if not fields: time.sleep(.03)
                assert fields, "native filename field unavailable"
                field = max(fields)[1]
                print("Native filename control:", user.GetDlgCtrlID(field), "directory:", directory, "selection:", names, flush=True)
                value = ctypes.create_unicode_buffer(str(names[0]) if directory
                    else " ".join('"' + str(path) + '"' for path in names))
                send(dialog, 0x0028, field, 1)  # WM_NEXTDLGCTL
                send(field, 0x000C, 0, ctypes.cast(value, ctypes.c_void_p).value)  # WM_SETTEXT
                echoed = ctypes.create_unicode_buffer(4096)
                send(field, 0x000D, 4096, ctypes.cast(echoed, ctypes.c_void_p).value)  # WM_GETTEXT across processes
                assert echoed.value == value.value, (echoed.value, value.value)
                time.sleep(.2)
            send(user.GetDlgItem(dialog, 2 if names is None else 1), 0x00F5)  # BM_CLICK
            closed = time.monotonic() + 5
            while user.IsWindow(dialog) and time.monotonic() < closed: time.sleep(.03)
            assert not user.IsWindow(dialog), "native selection remained open after confirmation"
        except BaseException as error:
            errors.append(error)
            if dialog: user.PostMessageW(dialog, 0x0010, 0, 0)
    worker = threading.Thread(target=drive, daemon=True)
    worker.start()
    try:
        click()
    finally:
        worker.join(20)
    assert not worker.is_alive(), "native input worker did not settle"
    if errors: raise errors[0]


def pattern(path):
    width, height = 480, 300
    pixels = b"".join(b"\0" + b"\xf0\x18\x18" * (width // 2)
                      + b"\x18\xf0\x18" * (width // 2) for _ in range(height))
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
                     + chunk(b"IDAT", zlib.compress(pixels)) + chunk(b"IEND", b""))


def verify(player, resources, audio_fixtures, image_fixtures, native_fixtures, output):
    from PIL import Image
    output.mkdir()
    project = build(output / "workspace")
    sources = output / "本地 选择"
    music, images, empty = (sources / name for name in ("音乐", "图片", "空目录"))
    for path in (music / "nested", images, empty): path.mkdir(parents=True)
    selected_music = []
    for index, suffix in enumerate(("wav", "mp3", "flac", "ogg", "m4a")):
        path = music / f"{index} 原创 音乐.{suffix}"
        shutil.copyfile(native_fixtures / "reference.m4a" if suffix == "m4a" else audio_fixtures / ("tone." + suffix), path)
        selected_music.append(path)
    shutil.copyfile(selected_music[0], music / "nested/duplicate.WAV")
    (music / "损坏.wav").write_bytes(b"RIFF broken")
    (music / "ignored.txt").write_text("not media", encoding="utf-8")
    picture = images / "0 原创图片.png"; pattern(picture)
    for suffix in ("jpg", "jpeg"): shutil.copyfile(image_fixtures / "color.jpg", images / ("1 图片." + suffix))
    source_hashes = {p.relative_to(sources).as_posix(): digest(p) for p in sources.rglob("*") if p.is_file()}
    os.environ["VK_INSTANCE_LAYERS"] = "VK_LAYER_KHRONOS_validation"
    os.environ["VK_LOADER_DEBUG"] = "layer"
    profile, control = output / "profile", {}
    def slot(name):
        paths = list(profile.glob("project-*/save-" + name + ".json"))
        return paths[0] if paths else None
    def payload(name):
        file = slot(name)
        return json.loads(snapshot_bytes(file))["payload"] if file else None
    def entries(kind):
        value = payload("local-media-" + kind)
        return value[1] if value and value[0] == 1 else []
    def wait_for(check):
        deadline = time.monotonic() + 20
        while not check():
            assert time.monotonic() < deadline, "media state did not settle"
            control["call"]("wait-frames", frames=5)
    def choose(click, action, kind, names):
        native_selection(control["pid"], "导入本地音乐" if kind == "music" else "导入本地图片",
                         names, lambda: click(action), directory="folder" in action)
    def begin(window, call): control.update(pid=window.pid, call=call)
    def colors(screenshot):
        bitmap = Image.open(screenshot).convert("RGB")
        pixels = list(bitmap.get_flattened_data() if hasattr(bitmap, "get_flattened_data") else bitmap.getdata())
        red = sum(r > 150 and g < 70 and b < 70 for r, g, b in pixels)
        green = sum(g > 150 and r < 70 and b < 70 for r, g, b in pixels)
        assert red > 1000 and green > 1000, (red, green)
        return dict(redPixels=red, greenPixels=green)

    with playing(player, project, resources, profile, output / "import", on_ready=begin) as (click, _, _, screenshot):
        screenshot("initial-workspace.png")
        click("media-play")
        choose(click, "media-import-music", "music", None)
        assert not slot("local-media-music")
        choose(click, "media-folder-music", "music", [empty])
        control["call"]("wait-frames", frames=10)
        assert not entries("music")
        choose(click, "media-import-music", "music", selected_music[:2])
        wait_for(lambda: len(entries("music")) == 2)
        choose(click, "media-folder-music", "music", [music])
        wait_for(lambda: len(entries("music")) == 5)
        choose(click, "media-import-images", "images", sorted(images.iterdir()))
        wait_for(lambda: len(entries("images")) == 2 and payload("local-media-view")[2] != "")
        choose(click, "media-folder-images", "images", [images])
        control["call"]("wait-frames", frames=20)
        image_colors = colors(screenshot("imported-images.png"))
        click("media-image-next"); click("media-image-prev")
        click("media-music-next")
        playback = []
        for _ in range(5):
            selected = payload("local-media-view")[1]
            row, = [row for row in entries("music") if row[0] == selected]
            click("media-play")
            state = json.loads(control["call"]("query", name="game.state"))
            assert state["audioStarts"] >= len(playback) + 1, state
            playback.append(dict(id=selected, format=row[2], audioStarts=state["audioStarts"]))
            click("media-play"); click("media-music-next")
        assert {item["format"] for item in playback} == {"wav", "mp3", "flac", "vorbis", "m4a"}, playback
        preference = payload("local-media-view")
        assert preference[3] is False and preference[1] in [row[0] for row in entries("music")]
        first_music, first_images = entries("music"), entries("images")
        screenshot("music-paused.png")
    assert source_hashes == {p.relative_to(sources).as_posix(): digest(p) for p in sources.rglob("*") if p.is_file()}
    # These two named directories are newly owned siblings inside this task.
    retired_sources = output / "sources-moved"
    moved_project = output / "workspace-moved"
    for source, destination in ((sources, retired_sources), (project.parent, moved_project)):
        assert source.resolve().parent == output.resolve() and destination.resolve().parent == output.resolve()
        assert not destination.exists()
        source.rename(destination)
    project = moved_project / "project.azureproject"
    with playing(player, project, resources, profile, output / "reopen", on_ready=begin) as (click, _, _, screenshot):
        assert payload("local-media-view") == preference
        assert entries("music") == first_music and entries("images") == first_images
        colors(screenshot("restored-images.png"))
        click("media-play"); click("media-play")
    png_row, = [row for row in first_images if row[2] == "png"]
    image_slot = slot("local-media-images")
    cached = image_slot.parent / "content-local-media-images" / png_row[3] / png_row[4]
    cached.rename(cached.with_suffix(".offline"))
    with playing(player, project, resources, profile, output / "recover", on_ready=begin) as (click, _, _, screenshot):
        moved_image = retired_sources / "图片" / picture.name
        choose(click, "media-import-images", "images", [moved_image])
        wait_for(lambda: any(row[0] == png_row[0] and row[3] != png_row[3] for row in entries("images")))
        control["call"]("wait-frames", frames=10)
        colors(screenshot("repaired-image.png"))
        music_slot = slot("local-media-music")
        original = snapshot_bytes(music_slot)
        for label, document in (("payload", {"schemaVersion": 1, "payload": [99, []]}),
                                ("envelope", {"schemaVersion": 99, "payload": [99, []]})):
            frozen = json.dumps(document, separators=(",", ":")).encode()
            music_slot.write_bytes(frozen)
            choose(click, "media-import-music", "music", [retired_sources / "音乐" / selected_music[0].name])
            control["call"]("wait-frames", frames=60)
            assert snapshot_bytes(music_slot) == frozen, label
            screenshot("protected-" + label + ".png")
            music_slot.write_bytes(original)
            click("media-refresh")
            click("media-play"); click("media-play")
        final_images = entries("images")
        assert len(final_images) == 2 and {row[0] for row in final_images} == {row[0] for row in first_images}
    for name in ("import", "reopen", "recover"):
        stderr = (output / name / "stderr.log").read_text(encoding="utf-8")
        assert 'Insert instance layer "VK_LAYER_KHRONOS_validation"' in stderr
        assert "VUID-" not in stderr and "SYNC-HAZARD" not in stderr
        assert "Could not load texture" not in stderr and "Failed to parse" not in stderr
    report = dict(playerSHA256=digest(player), nativeDialogs=9, independentPlayers=3, musicEntries=5, imageEntries=2,
                  formats=["wav", "mp3", "flac", "ogg", "m4a", "png", "jpg", "jpeg"], playback=playback, imageColors=image_colors,
                  sourceSHA256=source_hashes, viewPreference=preference, projectMoved=True, sourcesMoved=True,
                  missingImageRepaired=True, imageIdentitiesPreserved=True,
                  futurePayloadPreserved=True, futureEnvelopePreserved=True, liveAllocationsAfterExit=0,
                  controllerSHA256=digest(project.parent / "assets/media.lua"),
                  uiSHA256=digest(project.parent / "assets/media.rml"))
    write(output / "verification.json", report)
    print(json.dumps(report, ensure_ascii=False))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("player", "resources", "audio-fixtures", "image-fixtures", "native-fixtures", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    verify(args.player, args.resources, args.audio_fixtures, args.image_fixtures, args.native_fixtures, args.output)
