"""Diagnose the first chapter's conditional motions through the normal room UI."""

import argparse
from contextlib import contextmanager
import json
from pathlib import Path
import time

from test_notes_player import playing
from test_story_player import MotionTrace, PREFIX, compare_voices, opened_music, slot, wait_for_slot
from test_unlock_room_player import save
from tools.npr_room.build_story_signal_room import GRAPH, inputs, integration
from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest


@contextmanager
def entry(room, graph, program):
    tag = "capture-main-conditional"
    root = room / "assets"
    names = {kind: tag + suffix for kind, suffix in (
        ("boot", "-boot.azurelevel"), ("bootstrap", "-boot.lua"), ("level", ".azurelevel"),
        ("script", ".lua"), ("graph", ".json"))}
    paths = {key: "assets:/" + name for key, name in names.items()}
    source_bootstrap = (root / "startup-flow.lua").read_text(encoding="utf-8")
    startup = {}
    for stage in ("boot", "asset-load", "entry", "transition"):
        original = stage + ".azurelevel"
        alias = tag + "-" + original
        level = json.loads((root / original).read_bytes())
        bootstrap, = [node["components"]["azure.script"]["data"] for node in level["nodes"]
                      if "azure.script" in node["components"]]
        assert bootstrap["asset"] == "assets:/startup-flow.lua"
        bootstrap["asset"] = paths["bootstrap"]
        startup[root / alias] = json.dumps(level)
        if stage != "boot":
            assert source_bootstrap.count("assets:/" + original) == 1
            source_bootstrap = source_bootstrap.replace("assets:/" + original, "assets:/" + alias)
    assert source_bootstrap.count("assets:/room.azurelevel") == 1

    scene = json.loads((root / "room.azurelevel").read_bytes())
    animators, timers = 0, 0
    for node in scene["nodes"]:
        for kind, value in node["components"].items():
            data = value["data"]
            if kind == "azure.animator" and data.get("asset") == "assets:/" + GRAPH:
                data["asset"] = paths["graph"]
                animators += 1
            if kind == "azure.script" and data.get("asset") == "assets:/pomodoro.lua":
                data["asset"] = paths["script"]
                timers += 1
    assert (animators, timers) == (7, 1)
    descriptor = room / (tag + ".azureproject")
    project = json.loads((room / "project.azureproject").read_bytes())
    assert project["startupScene"] == "assets:/boot.azurelevel"
    project["startupScene"] = paths["boot"]
    fixture = {
        descriptor: json.dumps(project), **startup,
        root / names["bootstrap"]: source_bootstrap.replace("assets:/room.azurelevel", paths["level"]),
        root / names["level"]: json.dumps(scene), root / names["script"]: program,
        root / names["graph"]: json.dumps(graph),
    }
    created = []
    try:
        for path, text in fixture.items():
            with path.open("x", encoding="utf-8") as output:
                created.append(path)
                output.write(text)
        yield descriptor, paths["level"]
    finally:
        for path in reversed(created):
            path.unlink()


def replay(player, room, resources, catalog, graph, chapters, output):
    compiled, route, bindings = inputs(room, catalog, graph, chapters)
    original_script = room / "assets/pomodoro.lua"
    original_hash = digest(original_script)
    program = integration(original_script.read_text(encoding="utf-8"), route)
    trace = MotionTrace(room, chapters)
    output.mkdir()
    profile, run = output / "profile", output / "chapter"
    with entry(room, compiled, program) as (project, level):
        save(profile, project, "room-unlocks", [1, ["tutorial_01"], [], []])
        with playing(player, project, resources, profile, run, level_reference=level,
                     **trace.observe("chapter")) as (click, _, _, _):
            click("timer-mode-button")
            click("timer-toggle-button")
            time.sleep(1.25)
            click("timer-end-button")
            progress = wait_for_slot(profile, "work-calendar", lambda value: value[0] == 2 and value[2][0] == 2, 12)
            click(PREFIX + "start", wait=10)
            wait_for_slot(profile, "room-unlocks", lambda value: "main_01" in value[1], 110, trace.sample)
    assert digest(original_script) == original_hash
    authored = json.loads(chapters.read_bytes())["chapters"]
    voice_rows = [96, 97, 98, 99, 100, 101, 105, 106, 107, 108]
    compare_voices(run, authored, voice_rows)
    assert slot(profile, "room-unlocks")[1] == ["tutorial_01", "main_01"]
    assert slot(profile, "room-book-layout") == [1, "Book_1"]
    music = opened_music(run)
    tracks = json.loads((room / "music-receipt.json").read_bytes())["tracks"]
    named, = [Path(item["asset"]).name for item in tracks if item["title"] == "NEWings"]
    assert music == [Path(tracks[0]["asset"]).name, named], music
    assert "Allocator after unload: buffers=0 images=0" in (run / "stdout.log").read_text(encoding="utf-8")
    error = (run / "stderr.log").read_text(encoding="utf-8")
    assert "Syntax error parsing" not in error and "[warning][script]" not in error, error[-3500:]
    frames = trace.routes["chapter"]
    ordered = []
    for _, clip, _, _, _ in frames:
        if not ordered or ordered[-1] != clip:
            ordered.append(clip)
    sampled = {path.split(".")[-1]: record["clip"] for path, record in bindings.items()}
    expected = [
        "Motion5_Start_Shy", "Base001_Motion5_Shy", "Motion1_Start_FrustrationRight",
        "Base001_Motion1_FrustrationRight", "Motion1_End_FrustrationRight",
        "Motion3_Start_PressHands", "Base001_Motion3_PressHands", "Motion3_End_PressHands",
        "anim_story_subbase001_base_start", "Story_SubBase001", "Story_SubBase001_Fun",
    ]
    write(output / "chapter-frames.json", frames)
    cursor = iter(ordered)
    for name in expected:
        assert sampled[name] in cursor, (name, ordered)
    assert ordered.count(sampled["Story_SubBase001_Fun"]) == 1
    proof = {"playerSHA256": digest(player), "scriptSourceSHA256": original_hash, "orderedClips": ordered,
             "parts": len(trace.parts), "level": progress[2][0], "voiceRows": voice_rows, "music": music,
             "samples": len(frames), "route": expected, "finishedChapters": ["tutorial_01", "main_01"],
             "bookLayout": "Book_1", "cleanAllocatorExits": 1}
    write(output / "first-chapter-room-replay.json", proof)
    return proof


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parameters = ("player", "room", "resource-root", "catalog", "graph", "chapters", "output")
    for name in parameters:
        parser.add_argument("--" + name, type=Path, required=True)
    args = vars(parser.parse_args())
    print(json.dumps(replay(*(args[name.replace("-", "_")].resolve() for name in parameters))))
