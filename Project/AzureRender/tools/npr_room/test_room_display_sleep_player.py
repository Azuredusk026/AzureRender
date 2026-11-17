"""Exercise the Windows display power request through a normal room transition."""

import argparse
import json
from pathlib import Path
import time

from test_notes_player import playing
from test_story_signal_room_player import entry
from tools.npr_room.build_story_signal_room import GRAPH
from tools.npr_room.window_settings_room import install


def replay(player, project, resources, output):
    output.mkdir()
    room = project.parent
    assets = room / "assets"
    document = (assets / "timer.rml").read_text(encoding="utf-8")
    graph = json.loads((assets / GRAPH).read_bytes())
    timer = (assets / "pomodoro.lua").read_text(encoding="utf-8")
    session = {}

    def configure(descriptor, scene, prefix):
        configured = install(descriptor, scene, document, prefix)
        idle = prefix + "system-display-idle.azurelevel"
        session["idle"] = "assets:/" + idle
        alternate = scene | {"id": "system-display-idle", "nodes": [
            node for node in scene["nodes"] if node["id"] not in ("room-display", "room-display-window")
        ]}
        return configured | {idle: json.dumps(alternate)}

    def ready(_, call):
        session["call"] = call

    def visit(level, expected, switch=True):
        call = session["call"]
        if switch:
            call("input", domain="game", event={"action": "load-level", "reference": level})
        deadline = time.monotonic() + 40
        while True:
            state = json.loads(call("query", name="game.state"))
            assert not (state["loadError"] or state["scriptErrors"] or state["presentationErrors"]), state
            if state["levelReference"] == level and not state["loading"]:
                break
            assert time.monotonic() < deadline, level
            time.sleep(.05)
        call("wait-frames", frames=4)
        observed = call("query", name="window.displayWakeRequested")
        assert observed is expected, (level, expected, observed)
        return {"level": level, "displayWakeRequested": observed}

    with entry(room, graph, timer, additional_room=configure) as (fixture, level):
        with playing(player, fixture, resources, output / "profile", output / "transition",
                     level_reference=level, on_ready=ready):
            states = [visit(level, True, switch=False), visit(session["idle"], False), visit(level, True)]
    log = output / "transition" / "stdout.log"
    assert "Allocator after unload: buffers=0 images=0" in log.read_text(encoding="utf-8")
    report = {"windows": states, "cleanAllocatorExit": True}
    (output / "display-sleep-replay.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("player", "project", "resource-root", "output"):
        parser.add_argument("--" + option, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(replay(args.player.resolve(), args.project.resolve(),
                            args.resource_root.resolve(), args.output.resolve())))
