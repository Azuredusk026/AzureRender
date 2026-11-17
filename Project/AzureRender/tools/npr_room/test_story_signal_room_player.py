"""Diagnose the first chapter's conditional motions through the normal room UI."""

import argparse
from contextlib import contextmanager
import json
import os
from pathlib import Path
import time

from test_notes_player import playing
from test_story_player import MotionTrace, PREFIX, compare_voices, opened_music, slot, wait_for_slot
from test_story_signals_player import camera_model_links
from test_unlock_room_player import save
from tools.npr_room.build_story_signal_room import GRAPH, inputs, integration
from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest
from tools.npr_room.tutorial_camera_body import project as tutorial_project


@contextmanager
def entry(room, graph, program, models=None):
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
    links = {}
    if models:
        parts = [node for node in scene["nodes"] if node["id"].startswith("heroine-")
                 and "azure.animator" in node["components"]]
        links = camera_model_links(root, scene, parts, models, tag)
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
        for alias, original in links.items():
            os.link(original, alias)
            created.append(alias)
        for path, text in fixture.items():
            with path.open("x", encoding="utf-8") as output:
                created.append(path)
                output.write(text)
        yield descriptor, paths["level"]
    finally:
        for path in reversed(created):
            path.unlink()


def replay(player, room, resources, catalog, graph, chapters, output, source_receipt=None, models=None):
    if bool(source_receipt) != bool(models):
        raise ValueError("The tutorial replay needs both source receipt and sampled models")
    original_script = room / "assets/pomodoro.lua"
    original_hash = digest(original_script)
    compiled, route, bindings = inputs(room, catalog, graph, chapters)
    if models:
        compiled, program, _, camera = tutorial_project(room, catalog, graph, chapters, source_receipt, models)
    else:
        program = integration(original_script.read_text(encoding="utf-8"), route)
    trace = MotionTrace(room, chapters)
    output.mkdir()
    profile, run = output / "profile", output / "chapter"
    authored = json.loads(chapters.read_bytes())["chapters"]
    tracks = json.loads((room / "music-receipt.json").read_bytes())["tracks"]
    tutorial = None
    with entry(room, compiled, program, models) as (project, level):
        if models:
            startup = output / "tutorial"
            save(profile, project, "room-book-layout", [1, "Book_5"])
            with playing(player, project, resources, profile, startup, level_reference=level,
                         **trace.observe("tutorial")) as (click, press, _, _):
                click(PREFIX + "start")
                click(PREFIX + "option-1", wait=70)
                press(77, domain="game")
                assert slot(profile, "timer-mode") is None
                click(PREFIX + "heroine", wait=38)
                for option in (1, 2, 3, 4, 1):
                    click(PREFIX + f"option-{option}", wait=85)
                wait_for_slot(profile, "room-unlocks", lambda value: "tutorial_01" in value[1], 40, trace.sample)
            assert slot(profile, "room-book-layout") == [1, "Book_5"]
            assert opened_music(startup) == [Path(tracks[0]["asset"]).name]
            assert "Allocator after unload: buffers=0 images=0" in (startup / "stdout.log").read_text(encoding="utf-8")
            rows = [9, 24, 30, 39, 44, 45, 46, 48, 57, 59, 62, 64, 67, 69, 72, 73, 74, 75, 76, 80, 85, 86]
            compare_voices(startup, authored, rows)
            camera_clips = {state.split(".")[-1]: value["clip"] for state, value in camera.items()}
            states = ["GameStart_Start", "GameStartDirectionLoop", "GameStart_End", "Base001_Motion12_Start_Nod"]
            frames = trace.routes["tutorial"]
            write(output / "tutorial-frames.json", frames)
            cursor = iter(row[1] for row in frames)
            for name in states:
                assert camera_clips[name] in cursor, (name, sorted({row[1] for row in frames}))
            tutorial = {"samples": len(frames), "route": states, "voiceRows": rows, "bookSave": "Book_5",
                        "cleanAllocatorExits": 1}
        else:
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
    voice_rows = [96, 97, 98, 99, 100, 101, 105, 106, 107, 108]
    compare_voices(run, authored, voice_rows)
    assert slot(profile, "room-unlocks")[1] == ["tutorial_01", "main_01"]
    assert slot(profile, "room-book-layout") == [1, "Book_1"]
    music = opened_music(run)
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
    if tutorial:
        proof.update(tutorial=tutorial, cleanAllocatorExits=2)
    write(output / "first-chapter-room-replay.json", proof)
    return proof


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parameters = ("player", "room", "resource-root", "catalog", "graph", "chapters", "output")
    for name in parameters:
        parser.add_argument("--" + name, type=Path, required=True)
    for name in ("source-receipt", "models"):
        parser.add_argument("--" + name, type=Path)
    args = vars(parser.parse_args())
    print(json.dumps(replay(*(args[name.replace("-", "_")].resolve() for name in parameters),
                            source_receipt=args["source_receipt"], models=args["models"])))
