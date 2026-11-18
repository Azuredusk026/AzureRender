"""Diagnose the first chapter's conditional motions through the normal room UI."""

import argparse
from contextlib import contextmanager
import json
import os
from pathlib import Path
import time

import numpy as np
from PIL import Image

from test_notes_player import playing
from test_opening_camera_player import pose_error
from test_story_player import MotionTrace, PREFIX, compare_voices, opened_music, slot, wait_for_slot
from test_story_signals_player import camera_model_links
from test_unlock_room_player import save
from tools.npr_room.build_story_signal_room import GRAPH, inputs, integration
from tools.npr_room.opening_camera import attach_audio, project as camera_project
from tools.npr_room.opening_blackout import bind as blackout_program, bind_room as rendered_blackout
from tools.npr_room.opening_blackout import room_inputs as blackout_inputs
from tools.npr_room.opening_chromatic import bind as rendered_chromatic, room_inputs as chromatic_inputs
from tools.npr_room.opening_screen_move import bind as rendered_screen_move, room_inputs as screen_move_inputs
from tools.npr_room.opening_glitch_room import bind as rendered_glitch, room_inputs as glitch_inputs
from tools.npr_room.opening_face_layers import bind as face_program
from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest
from tools.npr_room.room_achievement_events import bind as achievement_programs
from tools.npr_room.story_chapters import lua_value
from tools.npr_room.tutorial_camera_body import project as tutorial_project


@contextmanager
def entry(room, graph, program, models=None, camera_sound=None, camera_source=None, room_scripts=None, blackout=None,
          additional_room=None, chromatic=None, screen_move=None, glitch=None, *, tag="capture-main-conditional"):
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
    if camera_sound:
        attach_audio(scene, camera_sound)
        audio_file = (camera_source / camera_sound["source"]).resolve(strict=True)
        assert audio_file.is_relative_to(camera_source / "Assets") and digest(audio_file) == camera_sound["sha256"]
        links[root / camera_sound["asset"]] = audio_file
    for effect in (blackout, chromatic, screen_move, glitch):
        if effect is None:
            continue
        node, media, _ = effect
        scene["nodes"].append(node)
        links.update({root / name: source for name, source in media.items()})
    animators, timers = 0, 0
    script_files = {}
    extra_scripts = room_scripts or {}
    for node in scene["nodes"]:
        for kind, value in node["components"].items():
            data = value["data"]
            if kind == "azure.animator" and data.get("asset") == "assets:/" + GRAPH:
                data["asset"] = paths["graph"]
                animators += 1
            if kind == "azure.script" and data.get("asset") == "assets:/pomodoro.lua":
                data["asset"] = paths["script"]
                timers += 1
            elif kind == "azure.script" and data.get("asset", "").removeprefix("assets:/") in extra_scripts:
                original = data["asset"].removeprefix("assets:/")
                alias = tag + "-" + original
                data["asset"] = "assets:/" + alias
                script_files[root / alias] = extra_scripts[original]
    assert (animators, timers) == (7, 1)
    if room_scripts and len(script_files) != len(room_scripts):
        raise ValueError("The room does not bind every requested controller")
    descriptor = room / (tag + ".azureproject")
    project = json.loads((room / "project.azureproject").read_bytes())
    assert project["startupScene"] == "assets:/boot.azurelevel"
    project["startupScene"] = paths["boot"]
    additional = {root / name: content for name, content in
                  additional_room(project, scene, tag + "-").items()} if additional_room else {}
    fixture = {
        descriptor: json.dumps(project), **startup, **script_files, **additional,
        root / names["bootstrap"]: source_bootstrap.replace("assets:/room.azurelevel", paths["level"]),
        root / names["level"]: json.dumps(scene), root / names["script"]: program,
        root / names["graph"]: json.dumps(graph),
    }
    created = []
    owned_metadata = []
    try:
        for alias, original in links.items():
            os.link(original, alias)
            created.append(alias)
            sidecar = alias.with_name(alias.name + ".azmeta")
            if not sidecar.exists():
                owned_metadata.append(sidecar)
        for path, text in fixture.items():
            with path.open("x", encoding="utf-8") as output:
                created.append(path)
                output.write(text)
            sidecar = path.with_name(path.name + ".azmeta")
            if not sidecar.exists():
                owned_metadata.append(sidecar)
        yield descriptor, paths["level"]
    finally:
        for path in reversed(created):
            path.unlink()
        for sidecar in owned_metadata:
            sidecar.unlink(missing_ok=True)


def camera_observer(track):
    return f"""
local cameraNode = {lua_value(track["node"])}
local cueNode = {lua_value(track["cueNode"]) if "cueNode" in track else "nil"}
local boundCamera = new_story_camera
local readyForFinish = false
function new_story_camera(host, data)
    local camera = boundCamera(host, data)
    local finish = camera.finish
    function camera:finish()
        finish(self)
        readyForFinish = true
    end
    return camera
end
local originalUpdate = update
local moved, rebound = false, false
local previousPlaying, cueStarts
function update(dt)
    local cue = cueNode and assert(self:find(cueNode))
    if cue then
        local playing = cue:audio_playing()
        if cueStarts == nil or (playing and not previousPlaying) then
            cueStarts = (cueStarts or 0) + (playing and 1 or 0)
            self:write_save("opening-camera-play-count", {{1, cueStarts}})
        end
        previousPlaying = playing
    end
    if not rebound then
        local actor = assert(self:find(cameraNode))
        local position = actor:get("azure.transform", "translation")
        if math.abs(position[2] - {track["translation"][1]:.12f}) < .0001 then
            local rotation = actor:get("azure.transform", "rotation")
            local movedAngle = {track["rotations"][40][0]:.12f}
            local finishAngle = {track["finish"][0]:.12f}
            local onPlateau = math.abs((rotation[1] - movedAngle + 180) % 360 - 180) < .0001
            if not moved and onPlateau then
                self:write_save("opening-camera-movement", {{1, position, rotation, cue and cue:audio_playing() or false}})
                moved = true
            end
            local atFinish = math.abs((rotation[1] - finishAngle + 180) % 360 - 180) < .00003
            if moved and readyForFinish and atFinish then
                self:write_save("opening-camera-finish", {{1, position, rotation, cue and cue:audio_playing() or false}})
                rebound = true
            end
        end
    end
    originalUpdate(dt)
end
"""


def face_observer(gaze_nodes, plank_nodes):
    return """
local originalFaceUpdate = update
local faceSample, voiceSample, finished = nil, nil, false
local gazeNodes = REPLAY_GAZE_NODES
local plankNodes = REPLAY_PLANK_NODES
local plankCleanPose
local plankDuring = 0
local gazeParts = 0
local gazeCleared = false
local sawExpression, initialized = false, false
function update(dt)
    if not initialized then
        self:write_save("opening-face-room-proof", {1, false})
        initialized = true
    end
    if not finished then
        local actor = assert(self:find("heroine-face"))
        local cue = assert(self:find("opening-story-voice"))
        local pose = actor:get("azure.animator", "expression")
        if pose:find('"morphs":', 1, true) then
            sawExpression = true
            local plankCount = 0
            for _, node in ipairs(plankNodes) do
                local value = assert(self:find(node)):get("azure.animator", "expression")
                if value:find('"bones":[{', 1, true) then plankCount = plankCount + 1 end
            end
            plankDuring = math.max(plankDuring, plankCount)
            if #gazeNodes > 0 and pose:find('"humanoidRotations":', 1, true) then
                local count = 0
                for _, node in ipairs(gazeNodes) do
                    local target = assert(self:find(node))
                    local value = target:get("azure.animator", "expression")
                    if value:find('"humanoidRotations":', 1, true) then count = count + 1 end
                end
                gazeParts = math.max(gazeParts, count)
            end
            for name, text in pose:gmatch('"targetId":"([^"]+)","weight":([%+%-%d%.eE]+)') do
                local weight = assert(tonumber(text))
                if math.abs(weight) > .025 then
                    if not faceSample and name:find("Eyeblow_", 1, true) then
                        faceSample = {name, weight}
                    end
                    if not voiceSample and cue:audio_playing() and
                        (name == "blendShape1.Mouth_i" or name == "blendShape1.Mouth_o" or name == "blendShape1.Mouth_a") then
                        voiceSample = {name, weight}
                    end
                end
            end
        elseif sawExpression then
            gazeCleared = true
            for _, node in ipairs(gazeNodes) do
                local target = assert(self:find(node))
                if target:get("azure.animator", "expression"):find('"humanoidRotations":', 1, true) then
                    gazeCleared = false
                end
            end
            local plankNow, plankAfter, plankMoving = {}, 0, 0
            for index, node in ipairs(plankNodes) do
                local value = assert(self:find(node)):get("azure.animator", "expression")
                plankNow[index] = value
                if value:find('"bones":[{', 1, true) then
                    plankAfter = plankAfter + 1
                    if plankCleanPose and plankCleanPose[index] ~= value then plankMoving = plankMoving + 1 end
                end
            end
            if plankCleanPose or #plankNodes == 0 then
                self:write_save("opening-face-room-proof", {1, true, faceSample or false, voiceSample or false,
                    gazeParts, gazeCleared, plankDuring, plankAfter, plankMoving})
                finished = true
            end
            plankCleanPose = plankNow
        end
    end
    originalFaceUpdate(dt)
end
""".replace("REPLAY_GAZE_NODES", lua_value(gaze_nodes)).replace("REPLAY_PLANK_NODES", lua_value(plank_nodes))


def face_proof(profile, trace, gaze_nodes, plank_nodes):
    value = wait_for_slot(profile, "opening-face-room-proof", lambda item: item[1] is True, 12, trace.sample)
    assert value[0] == 1 and value[2] and value[3], value
    report = {"face": value[2], "mouth": value[3], "cleared": value[1]}
    if gaze_nodes:
        assert value[4] == len(gaze_nodes) and value[5] is True, value
        report["gazeParts"] = value[4]
        report["gazeCleared"] = value[5]
    if plank_nodes:
        assert value[6:9] == [len(plank_nodes)] * 3, value
        report["bodyOverlay"] = {"partsDuring": value[6], "partsAfter": value[7], "partsMovingAfter": value[8]}
    return report


def blackout_observer(program, rendered):
    binder = rendered_blackout if rendered else blackout_program
    return binder(program, """(function()
        local changes = {}
        return function(ratio, blind, seconds)
            changes[#changes + 1] = {ratio, blind, seconds}
            self:write_save("opening-blackout-timing", {1, changes})
        end
    end)()""")


def blackout_pixels(control, full, partial):
    center = np.s_[230:480, 350:900, :3]
    with Image.open(control) as before, Image.open(full) as black, Image.open(partial) as transition:
        assert before.size == black.size == transition.size == (1280, 720)
        samples = [np.asarray(frame.convert("RGBA")) for frame in (before, black, transition)]
    scene = [frame[center] for frame in samples]
    full_levels = np.unique(scene[1].reshape(-1, 3), axis=0)
    assert full_levels.shape == (2, 3) and np.all(full_levels[0] == 0), full_levels
    assert np.all(full_levels[1] == full_levels[1, 0]) and 0 < full_levels[1, 0] <= 64, full_levels
    counts = [int(np.count_nonzero(np.any(frame != frame[..., :1], axis=2))) for frame in scene]
    assert counts[0] > 0 and counts[1] == 0 and 0 < counts[2] < 250 * 550, counts
    hud = np.any(samples[1][..., :3] != samples[1][..., :1], axis=2)
    assert hud.any() and np.array_equal(samples[1][hud], samples[2][hud]), "Blackout changed the room UI"
    return {"sceneRect": [350, 230, 900, 480], "coloredScenePixels": counts,
            "blackFrameGrayLevels": full_levels[:, 0].tolist(), "unchangedUiPixels": int(hud.sum()),
            "images": [digest(path) for path in (control, full, partial)]}


def chromatic_observer(program):
    return rendered_chromatic(program, """(function()
        local samples = {}
        return function(ratio, seconds)
            local count = #samples
            if (count == 0 and ratio > 0) or (count == 1 and ratio == .2)
               or (count == 2 and ratio < .2) or (count == 3 and ratio == 0) then
                samples[count + 1] = {ratio, seconds}
                self:write_save("opening-chromatic-timing", {1, samples})
            end
        end
    end)()""")


def chromatic_timing(profile):
    schema, samples = slot(profile, "opening-chromatic-timing")
    assert schema == 1 and len(samples) == 4, samples
    first, peak, decay, clear = samples
    assert 0 < first[0] < peak[0] == .2 and 0 < decay[0] < .2 and clear[0] == 0, samples
    rise_seconds = peak[1] - (first[1] - first[0] / .2 * 2)
    decay_seconds = clear[1] - (decay[1] - (.2 - decay[0]))
    assert abs(rise_seconds - 2) <= 2 / 60 and abs(decay_seconds - .2) <= 2 / 60, samples
    return samples, rise_seconds, decay_seconds


def screen_move_observer(program):
    return rendered_screen_move(program, """(function()
        local samples = {}
        return function(ratio, seconds)
            samples[#samples + 1] = {ratio, seconds}
            self:write_save("opening-screen-move-timing", {1, samples})
        end
    end)()""")


def glitch_observer(program):
    return rendered_glitch(program, """(function()
        local events = {}
        return function(kind, value, seconds)
            events[#events + 1] = {kind, value, seconds}
            self:write_save("opening-glitch-timing", {1, events})
        end
    end)()""")


def glitch_timing(profile):
    schema, events = slot(profile, "opening-glitch-timing")
    assert schema == 1
    ratios = [(value, seconds) for kind, value, seconds in events if kind == "ratio"]
    cameras = [(value, seconds) for kind, value, seconds in events if kind == "camera"]
    assert [row[0] for row in ratios] == [.5, 0, .3, 0], events
    holds = [ratios[1][1] - ratios[0][1], ratios[3][1] - ratios[2][1]]
    assert abs(holds[0] - 1) <= 2 / 60 and abs(holds[1] - .7) <= 2 / 60, events
    assert cameras[0][0] is True and cameras[-1][0] is False and len(cameras) >= 4, events
    assert all(a[0] != b[0] for a, b in zip(cameras, cameras[1:])), events
    assert all((True, start[1]) in cameras for start in ratios[::2]), events
    return events, holds


def replay(player, room, resources, catalog, graph, chapters, output, source_receipt=None, models=None,
           camera_trace=None, camera_audio_source=None, face_catalog=None, achievements=False, blackout_timing=False,
           blackout_source_lock=None, image_catalog=None, chromatic_source_lock=None, screen_move_source_lock=None,
           glitch_source_lock=None, glitch_camera=None):
    if bool(source_receipt) != bool(models):
        raise ValueError("The tutorial replay needs both source receipt and sampled models")
    original_script = room / "assets/pomodoro.lua"
    original_hash = digest(original_script)
    compiled, route, bindings = inputs(room, catalog, graph, chapters)
    if models:
        compiled, program, _, camera = tutorial_project(room, catalog, graph, chapters, source_receipt, models)
    else:
        program = integration(original_script.read_text(encoding="utf-8"), route)
    track, sound = None, None
    if camera_audio_source and not camera_trace:
        raise ValueError("Camera audio requires the original camera trace")
    if camera_trace:
        if not models:
            raise ValueError("The normal camera replay requires the tutorial body models")
        program, camera_receipt = camera_project(room, program, camera_trace, camera_audio_source)
        track = camera_receipt["track"]
        sound = camera_receipt.get("cue")
        program += camera_observer(track)
    gaze_nodes = []
    plank_nodes = []
    if face_catalog:
        if not models:
            raise ValueError("The real face replay requires body morphology")
        catalog_data = json.loads(face_catalog.read_bytes())
        gaze_nodes = [part["node"] for part in catalog_data["program"].get("look", {}).get("targets", ())]
        plank_nodes = [part["node"] for part in catalog_data["program"].get("plank", {}).get("targets", ())]
        program = face_program(program, catalog_data, models, chapters) + face_observer(gaze_nodes, plank_nodes)
    scripts, achieved = {}, []
    if achievements:
        if not models:
            raise ValueError("Achievement replay needs the normal tutorial body route")
        scripts = achievement_programs(program)
        program = scripts.pop("pomodoro.lua")
    if bool(blackout_source_lock or chromatic_source_lock or screen_move_source_lock or glitch_source_lock) != bool(image_catalog):
        raise ValueError("Rendered opening effects need a source lock and the image catalog")
    if bool(glitch_source_lock) != bool(glitch_camera):
        raise ValueError("Glitch replay requires the original camera receipt")
    rendered = blackout_inputs(blackout_source_lock, image_catalog) if blackout_source_lock else None
    if blackout_timing or rendered:
        program = blackout_observer(program, bool(rendered))
    chromatic = None
    if chromatic_source_lock:
        if not models:
            raise ValueError("Chromatic replay requires the normal tutorial body route")
        chromatic = chromatic_inputs(chromatic_source_lock, image_catalog)
        program = chromatic_observer(program)
    screen_move = None
    if screen_move_source_lock:
        if not models:
            raise ValueError("Screen move replay requires the normal tutorial body route")
        screen_move = screen_move_inputs(screen_move_source_lock, image_catalog)
        program = screen_move_observer(program)
    glitch = None
    if glitch_source_lock:
        if not models:
            raise ValueError("Glitch replay requires the normal tutorial body route")
        glitch = glitch_inputs(glitch_source_lock, image_catalog, glitch_camera)
        program = glitch_observer(program)
    face_results = {}
    trace = MotionTrace(room, chapters)
    output.mkdir()
    profile, run = output / "profile", output / "chapter"
    authored = json.loads(chapters.read_bytes())["chapters"]
    tracks = json.loads((room / "music-receipt.json").read_bytes())["tracks"]
    tutorial = None
    with entry(room, compiled, program, models, sound, camera_audio_source, scripts, rendered,
               chromatic=chromatic, screen_move=screen_move, glitch=glitch) as (project, level):
        if models:
            startup = output / "tutorial"
            save(profile, project, "room-book-layout", [1, "Book_5"])
            with playing(player, project, resources, profile, startup, level_reference=level,
                         **trace.observe("tutorial")) as (click, press, type_text, screenshot):
                if rendered:
                    control = screenshot("blackout-control.png")
                click(PREFIX + "start")
                if rendered:
                    wait_for_slot(profile, "opening-blackout-timing", lambda value: value[1][-1][0] == 1, 12, trace.sample)
                    full = screenshot("blackout-full.png")
                    wait_for_slot(profile, "opening-blackout-timing", lambda value: value[1][-1][0] == .5, 15, trace.sample)
                    partial = screenshot("blackout-partial.png")
                    black_pixels = blackout_pixels(control, full, partial)
                if chromatic:
                    wait_for_slot(profile, "opening-chromatic-timing", lambda value: len(value[1]) >= 2,
                                  15, trace.sample)
                    effect_frame = screenshot("chromatic-active.png")
                click(PREFIX + "option-1", wait=70)
                press(77, domain="game")
                assert slot(profile, "timer-mode") is None
                click(PREFIX + "heroine", wait=38)
                for option in (1, 2, 3, 4, 1):
                    click(PREFIX + f"option-{option}", wait=85)
                wait_for_slot(profile, "room-unlocks", lambda value: "tutorial_01" in value[1], 40, trace.sample)
                if face_catalog:
                    face_results["tutorial"] = face_proof(profile, trace, gaze_nodes, plank_nodes)
                if blackout_timing or rendered:
                    version, blackout = slot(profile, "opening-blackout-timing")
                    assert version == 1 and [(row[0], row[1]) for row in blackout] == [
                        (1, False), (.5, False), (0, False)], blackout
                    assert abs(blackout[2][2] - blackout[1][2] - .8) <= 2 / 60, blackout
                if chromatic:
                    effect_samples, rise_seconds, decay_seconds = chromatic_timing(profile)
                if screen_move:
                    schema, screen_move_events = slot(profile, "opening-screen-move-timing")
                    assert schema == 1 and [event[0] for event in screen_move_events] == [.5, 0], screen_move_events
                    movement_hold = screen_move_events[1][1] - screen_move_events[0][1]
                    assert abs(movement_hold - 1) <= 2 / 60, screen_move_events
                if glitch:
                    glitch_events, glitch_holds = glitch_timing(profile)
                if achievements:
                    tutorial_key = "COMPLETE_TUTORIAL_COUNT"
                    assert slot(profile, "room-achievements") == [1, [tutorial_key]]
                    click("todo-toggle")
                    click("todo-task-new")
                    click("todo-task-complete")
                    click("todo-task-complete")
                    click("todo-toggle")
                    click("notes-toggle")
                    click("note-text")
                    type_text("Room replay")
                    click("note-save")
                    click("notes-toggle")
                    achieved = [tutorial_key, "COMPLETE_TODO_COUNT", "END_EDIT_NOTE_COUNT"]
                    assert slot(profile, "room-achievements") == [1, achieved]
            assert slot(profile, "room-book-layout") == [1, "Book_5"]
            assert opened_music(startup) == [Path(tracks[0]["asset"]).name]
            assert "Allocator after unload: buffers=0 images=0" in (startup / "stdout.log").read_text(encoding="utf-8")
            rows = [9, 24, 30, 39, 44, 45, 46, 48, 57, 59, 62, 64, 67, 69, 72, 73, 74, 75, 76, 80, 85, 86]
            compare_voices(startup, authored, rows)
            if track:
                unity = json.loads(camera_trace.read_bytes())
                start_schema, position, movement, cue_playing = slot(profile, "opening-camera-movement")
                end_schema, endpoint, rotation, cue_finished = slot(profile, "opening-camera-finish")
                assert start_schema == end_schema == 1
                anchor = unity["anchor"]["position"]
                origin = [value - local for value, local in zip(
                    (anchor["x"], anchor["y"], -anchor["z"]), track["translation"], strict=True)]
                start_error = pose_error(position, movement, unity["samples"][40], origin)
                end_error = pose_error(endpoint, rotation, unity["disabled"], origin)
                assert start_error[0] < 1e-6 and start_error[1] < .002, start_error
                assert end_error[0] < 1e-6 and end_error[1] < 3e-5, end_error
                if sound:
                    assert slot(profile, "opening-camera-play-count") == [1, 1]
                    assert cue_playing is True and cue_finished is False
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
            if achievements:
                assert slot(profile, "room-achievements") == [1, achieved]
            click("timer-mode-button")
            click("timer-toggle-button")
            time.sleep(1.25)
            click("timer-end-button")
            progress = wait_for_slot(profile, "work-calendar", lambda value: value[0] == 2 and value[2][0] == 2, 12)
            if achievements:
                achieved.append("COMPLETE_POMODORO_COUNT")
                assert slot(profile, "room-achievements") == [1, achieved]
            click(PREFIX + "start", wait=10)
            wait_for_slot(profile, "room-unlocks", lambda value: "main_01" in value[1], 110, trace.sample)
            if face_catalog:
                face_results["chapter"] = face_proof(profile, trace, gaze_nodes, plank_nodes)
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
    if sound:
        name = Path(sound["asset"]).name
        openings = lambda folder: sum(" source: " in row and Path(row.rsplit(" source: ", 1)[1]).name == name
                                      for row in (folder / "stderr.log").read_text(encoding="utf-8").splitlines())
        assert (openings(startup), openings(run)) == (1, 1)
        assert slot(profile, "opening-camera-play-count") == [1, 0]
        proof["cameraAudio"] = sound | {"tutorialStarts": 1, "firstChapterStarts": 0}
    if track:
        proof["camera"] = {"sourceTraceSHA256": digest(camera_trace),
                           "movementPoseError": start_error, "finishPoseError": end_error}
    if face_catalog:
        proof["facialLayers"] = {"catalogSHA256": digest(face_catalog), "responses": face_results}
    if achievements:
        assert slot(profile, "room-achievements") == [1, achieved]
        proof["localAchievements"] = {"keys": achieved, "reloaded": True}
    if blackout_timing or rendered:
        assert slot(profile, "opening-blackout-timing") == [1, blackout]
        proof["blackoutTiming"] = {"tutorial": blackout, "firstChapter": [], "rendered": bool(rendered)}
        if rendered:
            proof["blackoutTiming"]["pixels"] = black_pixels
            proof["blackoutTiming"]["material"] = rendered[2]
    if chromatic:
        assert slot(profile, "opening-chromatic-timing") == [1, effect_samples]
        proof["chromaticTiming"] = {"tutorial": effect_samples, "firstChapter": [], "riseSeconds": rise_seconds,
                                    "decaySeconds": decay_seconds, "imageSHA256": digest(effect_frame),
                                    "material": chromatic[2]}
    if screen_move:
        assert slot(profile, "opening-screen-move-timing") == [1, screen_move_events]
        proof["screenMoveTiming"] = {"tutorial": screen_move_events, "firstChapter": [], "holdSeconds": movement_hold,
                                     "material": screen_move[2]}
    if glitch:
        assert slot(profile, "opening-glitch-timing") == [1, glitch_events]
        proof["glitchTiming"] = {"tutorial": glitch_events, "firstChapter": [], "holdSeconds": glitch_holds,
                                 "material": glitch[2]}
    write(output / "first-chapter-room-replay.json", proof)
    return proof


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parameters = ("player", "room", "resource-root", "catalog", "graph", "chapters", "output")
    for name in parameters:
        parser.add_argument("--" + name, type=Path, required=True)
    for name in ("source-receipt", "models"):
        parser.add_argument("--" + name, type=Path)
    parser.add_argument("--camera-trace", type=Path)
    parser.add_argument("--camera-audio-source", type=Path)
    parser.add_argument("--face-catalog", type=Path)
    parser.add_argument("--achievements", action="store_true")
    parser.add_argument("--blackout-timing", action="store_true")
    parser.add_argument("--blackout-source-lock", type=Path)
    parser.add_argument("--chromatic-source-lock", type=Path)
    parser.add_argument("--screen-move-source-lock", type=Path)
    parser.add_argument("--glitch-source-lock", type=Path)
    parser.add_argument("--glitch-camera", type=Path)
    parser.add_argument("--image-catalog", type=Path)
    args = vars(parser.parse_args())
    print(json.dumps(replay(*(args[name.replace("-", "_")].resolve() for name in parameters),
                            source_receipt=args["source_receipt"], models=args["models"],
                            camera_trace=args["camera_trace"],
                            camera_audio_source=args["camera_audio_source"], face_catalog=args["face_catalog"],
                            achievements=args["achievements"], blackout_timing=args["blackout_timing"],
                            blackout_source_lock=args["blackout_source_lock"], image_catalog=args["image_catalog"],
                            chromatic_source_lock=args["chromatic_source_lock"],
                            screen_move_source_lock=args["screen_move_source_lock"],
                            glitch_source_lock=args["glitch_source_lock"], glitch_camera=args["glitch_camera"])))
