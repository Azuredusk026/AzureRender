"""Verify moved media packages through the Player, native entry and command entry."""
import argparse
import json
import os
from pathlib import Path
import struct
import subprocess
import time
import wave

from PIL import Image
from build_game import digest, verify_package
from test_game_launcher_save_root import isolated_environment
from test_local_media_player import native_selection, snapshot_bytes
from npr_room.test_notes_player import playing


def write(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def verify(package, profile, output, source_project, source_install):
    assert not source_project.exists() and not source_install.exists()
    manifest = verify_package(package)
    output.mkdir()
    env = isolated_environment()
    env["AZURERENDER_SAVE_ROOT"] = str(profile)
    for key in list(os.environ):
        if key.upper() in {"PATH", "PYTHONPATH", "AZURERENDER_RESOURCE_ROOT", "AZURERENDER_SAVE_ROOT"}:
            del os.environ[key]
    os.environ.update(env)
    control = {}
    def ready(window, call): control.update(pid=window.pid, call=call)
    def payload(slot):
        file, = profile.glob("project-*/save-" + slot + ".json")
        return json.loads(snapshot_bytes(file))["payload"]
    def rows(kind): return payload("local-media-" + kind)[1]
    initial_music, initial_images = rows("music"), rows("images")
    assert initial_music and initial_images
    def wait_for(check):
        deadline = time.monotonic() + 20
        while not check():
            assert time.monotonic() < deadline, "Media state did not settle"
            control["call"]("wait-frames", frames=5)
    source = output / "包内入口导入.wav"
    with wave.open(str(source), "wb") as audio:
        audio.setparams((1, 2, 48000, 0, "NONE", "not compressed"))
        amplitude = 3000 + len(initial_music)
        audio.writeframes(b"".join(struct.pack("<h", amplitude if (i // 48) % 2 else -amplitude) for i in range(96000)))
    with playing(package / "bin/AzurePlayer.exe", package / "game/project.azureproject",
                 package / "share/AzureRender", profile, output / "moved-player", on_ready=ready) as (click, _, _, screenshot):
        native_selection(control["pid"], "导入本地音乐", [source], lambda: click("media-import-music"))
        wait_for(lambda: len(rows("music")) == len(initial_music) + 1)
        imported, = [row for row in rows("music") if row[0] not in {item[0] for item in initial_music}]
        for _ in range(len(rows("music"))):
            if payload("local-media-view")[1] == imported[0]: break
            click("media-music-next")
        assert payload("local-media-view")[1] == imported[0]
        if payload("local-media-view")[3]: click("media-play")
        click("media-play")
        state = json.loads(control["call"]("query", name="game.state"))
        assert state["audioStarts"] >= 1, state
        if payload("local-media-view")[3]: click("media-play")
        assert payload("local-media-view")[3] is False
        bitmap = Image.open(screenshot("moved-media.png")).convert("RGB")
        pixels = list(bitmap.get_flattened_data() if hasattr(bitmap, "get_flattened_data") else bitmap.getdata())
        colors = dict(redPixels=sum(r > 150 and g < 70 and b < 70 for r, g, b in pixels),
                      greenPixels=sum(g > 150 and r < 70 and b < 70 for r, g, b in pixels))
        assert min(colors.values()) > 1000, colors
    saved_music, saved_images = rows("music"), rows("images")
    source_digest = digest(source)
    retired_source = source.with_suffix(".wav.unavailable")
    source.rename(retired_source)
    assert not source.exists() and digest(retired_source) == source_digest
    cases = []
    for label, entry in (("native", manifest["entry"]), ("command", "start-game.cmd")):
        stage = output / label
        stage.mkdir()
        actions = stage / "actions.json"
        write(actions, {"schemaVersion": 1, "actions": [
            {"frame": 6, "action": "key", "key": 32, "down": True},
            {"frame": 8, "action": "key", "key": 32, "down": False}]})
        command = ([str(package / entry)] if label == "native" else [env["COMSPEC"], "/d", "/c", "start-game.cmd"])
        command += ["--no-error-dialog", "--fixed-frame-step", "--smoke-frames", "30",
                    "--game-actions", str(actions), "--runtime-report", str(stage / "runtime.json")]
        result = subprocess.run(command, cwd=package, env=env, capture_output=True, timeout=100)
        (stage / "stdout.log").write_bytes(result.stdout + result.stderr)
        log = (package / "captures/game-startup.log").read_text(encoding="utf-8")
        (stage / "startup.log").write_text(log, encoding="utf-8")
        assert result.returncode == 0, (result.returncode, log[-3000:])
        state = json.loads((stage / "runtime.json").read_bytes())
        assert not state["scriptErrors"] and not state["presentationErrors"] and state["uiDrawCalls"] > 0, state
        assert state["audioStarts"] >= 1, "Moved entry did not restore and play persistent media"
        assert rows("music") == saved_music and rows("images") == saved_images
        assert "Allocator after unload: buffers=0 images=0" in log
        assert 'Insert instance layer "VK_LAYER_KHRONOS_validation"' in log
        assert not any(error in log for error in ("VUID-", "SYNC-HAZARD", "Validation Error"))
        cases.append(dict(entry=entry, command=command, exitCode=result.returncode,
                          audioStarts=state["audioStarts"], uiDrawCalls=state["uiDrawCalls"]))
    gui_log = (output / "moved-player/stderr.log").read_text(encoding="utf-8")
    assert 'Insert instance layer "VK_LAYER_KHRONOS_validation"' in gui_log
    assert not any(error in gui_log for error in ("VUID-", "SYNC-HAZARD", "Validation Error"))
    verify_package(package)
    summary = dict(package=str(package), profile=str(profile), sourceProject=str(source_project),
                   sourceInstall=str(source_install), sourceProjectUnavailable=True, sourceInstallUnavailable=True,
                   sourceMediaUnavailable=True, mediaSourceSHA256=source_digest, systemOnlyPath=env["PATH"],
                   projectId=manifest["projectId"], manifestSHA256=digest(package / "game_manifest.json"),
                   playerSHA256=digest(package / "bin/AzurePlayer.exe"), launcherSHA256=digest(package / manifest["entry"]),
                   fileCount=len(manifest["files"]), musicEntries=len(saved_music), imageEntries=len(saved_images),
                   independentPlayers=3, nativeDialogs=1, imageColors=colors, entries=cases,
                   manifestAfterReplay="passed", vulkanValidation="passed", resourcesAfterExit="buffers=0 images=0")
    write(output / "verification.json", summary)
    print(json.dumps(summary), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("package", "profile", "output", "source-project", "source-install"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    verify(args.package.resolve(), args.profile.resolve(), args.output.resolve(),
           args.source_project.resolve(), args.source_install.resolve())
