"""Verify persistent media through the native game launcher and explicit overrides."""
import argparse
import json
import math
import os
from pathlib import Path
import shutil
import struct
import subprocess
import time
import uuid
import wave

from build_local_media_example import build
from test_local_media_player import native_selection, pattern, snapshot_bytes
from npr_room.test_notes_player import playing
from npr_room.test_original_music_player import digest, write


def isolated_environment():
    result = {k: v for k, v in os.environ.items() if k.upper() not in {
        "PATH", "PYTHONPATH", "AZURERENDER_RESOURCE_ROOT", "AZURERENDER_SAVE_ROOT"}}
    system = Path(os.environ["SystemRoot"])
    result["PATH"] = str(system / "System32") + os.pathsep + str(system)
    result["VK_INSTANCE_LAYERS"] = "VK_LAYER_KHRONOS_validation"
    result["VK_LOADER_DEBUG"] = "layer"
    return result


def prepare(runtime, output):
    output.mkdir()
    project = build(runtime / "probe-game")
    description = json.loads(project.read_bytes())
    description["id"] = str(uuid.uuid4())
    description["runtime"]["input"]["media-play"] = [32]
    write(project, description)
    launcher = runtime / "Probe.exe"
    shutil.copyfile(runtime / "bin/AzureGameLauncher.exe", launcher)
    write(runtime / "game-launch.json", {"schemaVersion": 1, "player": "bin/AzurePlayer.exe",
          "project": "probe-game/project.azureproject", "resources": "share/AzureRender", "width": 1280, "height": 720})
    tone = output / "原创测试.wav"
    with wave.open(str(tone), "wb") as audio:
        audio.setparams((1, 2, 48000, 0, "NONE", "not compressed"))
        audio.writeframes(b"".join(struct.pack("<h", int(4000 * math.sin(i * math.tau * 330 / 48000))) for i in range(96000)))
    picture = output / "原创图片.png"; pattern(picture)
    profile, control = output / "profile", {}
    os.environ.update(isolated_environment())
    def ready(window, call): control.update(pid=window.pid, call=call)
    with playing(runtime / "bin/AzurePlayer.exe", project, runtime / "share/AzureRender", profile,
                 output / "prepare-player", on_ready=ready) as (click, _, _, screenshot):
        for kind, source, action in (("music", tone, "media-import-music"), ("images", picture, "media-import-images")):
            native_selection(control["pid"], "导入本地音乐" if kind == "music" else "导入本地图片",
                             [source], lambda: click(action))
            deadline = time.monotonic() + 15
            while not list(profile.glob("project-*/save-local-media-" + kind + ".json")):
                assert time.monotonic() < deadline
                control["call"]("wait-frames", frames=4)
        control["call"]("wait-frames", frames=10)
        screenshot("prepared-profile.png")
    write(output / "setup.json", dict(runtime=str(runtime), project=str(project), profile=str(profile),
          projectId=description["id"], playerSHA256=digest(runtime / "bin/AzurePlayer.exe")))
    print("Native runtime view and persistent media profile prepared")


def verify(output, case, label):
    setup = json.loads((output / "setup.json").read_bytes())
    runtime, profile = Path(setup["runtime"]), Path(setup["profile"])
    stage = output / label; stage.mkdir()
    env = isolated_environment()
    report = stage / "runtime.json"
    command = [str(runtime / "Probe.exe"), "--no-error-dialog", "--fixed-frame-step", "--smoke-frames", "30",
               "--runtime-report", str(report)]
    if case != "readonly":
        actions = stage / "actions.json"
        write(actions, {"schemaVersion": 1, "actions": [
            {"frame": 6, "action": "key", "key": 32, "down": True},
            {"frame": 8, "action": "key", "key": 32, "down": False}]})
        command += ["--game-actions", str(actions)]
    if case == "env": env["AZURERENDER_SAVE_ROOT"] = str(profile)
    if case == "explicit":
        env["AZURERENDER_SAVE_ROOT"] = str(output / "unused-profile")
        command += ["--save-root", str(profile)]
    if case == "invalid": env["AZURERENDER_SAVE_ROOT"] = "relative-profile"
    default_project = Path(os.environ["LOCALAPPDATA"]) / "AzureRender/GameSaves" / ("project-" + setup["projectId"].encode().hex())
    if case == "readonly": assert not default_project.exists(), "The read-only default-root probe needs a fresh project identity"
    startup_log = runtime / "captures/game-startup.log"
    previous_log = startup_log.read_bytes() if startup_log.exists() else None
    result = subprocess.run(command, cwd=stage, env=env, capture_output=True, timeout=100)
    current_log = startup_log.read_bytes() if startup_log.exists() else None
    log_changed = current_log != previous_log
    log = current_log.decode("utf-8", errors="replace") if current_log is not None and case != "invalid" else ""
    if case != "invalid": (stage / "startup.log").write_text(log, encoding="utf-8")
    state = json.loads(report.read_bytes()) if report.is_file() else {}
    summary = dict(case=case, command=command, exitCode=result.returncode, audioStarts=state.get("audioStarts"),
                   uiDrawCalls=state.get("uiDrawCalls"), scriptErrors=state.get("scriptErrors"),
                   presentationErrors=state.get("presentationErrors"), systemOnlyPath=env["PATH"],
                   launcherSHA256=digest(runtime / "Probe.exe"), playerSHA256=setup["playerSHA256"], startupLogChanged=log_changed)
    write(stage / "verification.json", summary)
    print(json.dumps(summary), flush=True)
    if case == "invalid":
        assert result.returncode != 0 and not report.exists() and not (stage / "relative-profile").exists() and not log_changed
        return
    assert result.returncode == 0, log[-3000:]
    assert not state["scriptErrors"] and not state["presentationErrors"] and state["uiDrawCalls"] > 0, state
    assert "Allocator after unload: buffers=0 images=0" in log
    assert 'Insert instance layer "VK_LAYER_KHRONOS_validation"' in log
    assert "VUID-" not in log and "SYNC-HAZARD" not in log
    if case == "readonly": assert not default_project.exists(), "Read-only startup must preserve the absent save directory"
    else: assert state["audioStarts"] >= 1, "The launcher must restore media from its selected persistent root"
    if case == "explicit": assert not (output / "unused-profile").exists()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--prepare", action="store_true")
    parser.add_argument("--case", choices=("env", "explicit", "readonly", "invalid"))
    parser.add_argument("--label", default="verification")
    args = parser.parse_args()
    if args.prepare: prepare(args.runtime, args.output)
    else: verify(args.output, args.case, args.label)
