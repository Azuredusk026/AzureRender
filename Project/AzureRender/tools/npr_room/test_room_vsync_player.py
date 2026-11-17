"""Verify the original Windows V-Sync setting against the Player swapchain."""

import argparse
import json
from pathlib import Path
import time

from test_notes_player import playing
from test_story_player import slot, wait_for_slot
from test_story_signal_room_player import entry
from tools.npr_room.build_story_signal_room import GRAPH
from tools.npr_room.window_settings_room import install


def replay(player, project, resources, output):
    output.mkdir()
    profile = output / "profile"
    room = project.parent
    assets = room / "assets"
    document = (assets / "timer.rml").read_text(encoding="utf-8")
    graph = json.loads((assets / GRAPH).read_bytes())
    timer = (assets / "pomodoro.lua").read_text(encoding="utf-8")
    session = {}

    def configure(descriptor, scene, prefix):
        return install(descriptor, scene, document, prefix)

    def ready(_, call):
        session["call"] = call

    def observe(sync, expected_modes):
        call = session["call"]
        deadline = time.monotonic() + 8
        while True:
            value = json.loads(call("query", name="window.surfacePresentation"))
            if value["vsync"] is sync and value["mode"] in expected_modes:
                break
            assert time.monotonic() < deadline, (sync, expected_modes, value)
            time.sleep(.03)
        pacing = json.loads(call("query", name="engine.framePacing"))
        return value | {"softwareFrameLimit": pacing["maximumRate"], "focused": pacing["focused"]}

    with entry(room, graph, timer, additional_room=configure) as (fixture, level):
        modes = {"FIFO", "MAILBOX"}
        with playing(player, fixture, resources, profile, output / "edit", level_reference=level,
                     on_ready=ready) as (click, _, _, _):
            disabled = observe(False, modes)
            assert disabled["softwareFrameLimit"] == (60 if disabled["focused"] else 24), disabled
            click("display-toggle")
            click("display-vsync")
            wait_for_slot(profile, "room-vsync", lambda value: value == [1, True], 5)
            enabled = observe(True, {"FIFO"})
            assert enabled["softwareFrameLimit"] == 0, enabled
            click("display-vsync")
            wait_for_slot(profile, "room-vsync", lambda value: value == [1, False], 5)
            restored = observe(False, {disabled["mode"]})
            assert restored["softwareFrameLimit"] == (60 if restored["focused"] else 24), restored
            click("display-vsync")
            wait_for_slot(profile, "room-vsync", lambda value: value == [1, True], 5)
            observe(True, {"FIFO"})
        with playing(player, fixture, resources, profile, output / "reload", level_reference=level,
                     on_ready=ready):
            reopened = observe(True, {"FIFO"})
            assert reopened["softwareFrameLimit"] == 0 and slot(profile, "room-vsync") == [1, True], reopened
        with playing(player, fixture, resources, profile, output / "override", level_reference=level,
                     on_ready=ready, player_args=("--set", "runtime.vsync=false", "--set", "runtime.maxFrameRate=15")):
            overridden = observe(False, {disabled["mode"]})
            assert overridden["softwareFrameLimit"] == 15 and slot(profile, "room-vsync") == [1, True], overridden
    for name in ("edit", "reload", "override"):
        assert "Allocator after unload: buffers=0 images=0" in (output / name / "stdout.log").read_text(encoding="utf-8")
    report = {"states": [disabled, enabled, restored, reopened, overridden], "cleanAllocatorExits": 3}
    (output / "room-vsync-replay.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("player", "project", "resource-root", "output"):
        parser.add_argument("--" + option, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(replay(args.player.resolve(), args.project.resolve(),
                            args.resource_root.resolve(), args.output.resolve())))
