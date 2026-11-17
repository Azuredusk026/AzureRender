"""Bake the two source camera motions and compile the opening body routes."""

import argparse
import gzip
import json
import math
import os
from pathlib import Path
import shutil
import tempfile

from tools.build_face_sdf import parse_glb
from tools.npr_room.build_story_book_room import skip_capture_orphans
from tools.npr_room.build_story_signal_room import GRAPH, compile_signals, inputs, integration, load
from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.humanoid_clip_export import export_model
from tools.npr_room.opening_camera import attach_audio, project as camera_project
from tools.npr_room.opening_blackout import bind_room as bind_blackout, room_inputs as blackout_inputs
from tools.npr_room.opening_chromatic import bind as bind_chromatic, room_inputs as chromatic_inputs
from tools.npr_room.opening_screen_move import bind as bind_screen_move, room_inputs as screen_move_inputs
from tools.npr_room.opening_face_layers import bind as face_program
from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest
from tools.npr_room.room_achievement_events import bind as achievement_programs
from tools.npr_room.room_geometry import HEROINE_PARTS, copy_room_asset, stage_live_audio_media
from tools.npr_room.story_body_morphs import MORPH_RECEIPT
from tools.npr_room.window_settings_room import bind_display_language, display_localization
from tools.npr_room.window_settings_room import install as install_window_settings


TUTORIAL_RECEIPT = "tutorial-camera-body-receipt.json"


def bake(room, rig, source_receipt, samples, output):
    source = load(source_receipt)
    if source["rigSHA256"] != digest(rig):
        raise ValueError("Expected the original Avatar rig used in the source capture")
    captured = json.loads(gzip.decompress(samples.read_bytes()))
    clips = captured["clips"]
    originals = {Path(path).stem: record for path, record in source["cameraClips"].items()}
    if captured["version"] != "2022.3.62f2c1" or len(clips) != 2 or {c["name"] for c in clips} != originals.keys():
        raise ValueError("Expected the two original camera clips from the reference editor")
    for clip in clips:
        source_clip = originals[clip["name"]]
        seconds = source_clip["stop"] - source_clip["start"]
        if (not clip["human"] or clip["rate"] != 60 or not clip["frames"]
                or not any(math.isclose(clip["length"], seconds + delta, abs_tol=1e-5) for delta in (0, 1 / 60))):
            raise ValueError("Unexpected camera clip timing")
    rig_nodes = load(rig)["nodes"]
    with tempfile.TemporaryDirectory(prefix="tutorial-camera-poses-", dir=output.parent) as workspace:
        project = Path(workspace) / "project"
        models = project / "assets/heroine"
        models.mkdir(parents=True)
        receipt = load(room / "opening-body-receipt.json")
        parts, order = {}, None
        for part in HEROINE_PARTS:
            before = room / "assets/heroine" / (part + ".glb")
            if digest(before) != receipt["parts"][part]["sha256"]:
                raise ValueError(f"Unverified input model: {part}")
            document, _ = parse_glb(before)
            names = [clip["name"] for clip in document["animations"]]
            if order is None:
                order = names
            if names != order or originals.keys() & set(names):
                raise ValueError(f"Conflicting camera clip indices: {part}")
            after = models / before.name
            channels = export_model(before, after, rig_nodes, clips)
            parts[part] = {"sha256": digest(after), "channels": channels}
        mapped = {clip["name"]: {"name": clip["name"], "clipIndex": len(order) + index,
                                  "seconds": clip["length"], "sourceSHA256": originals[clip["name"]]["sha256"]}
                  for index, clip in enumerate(clips)}
        profile = {"schemaVersion": 1, "sourceReceiptSHA256": digest(source_receipt), "rigSHA256": digest(rig),
                   "parts": parts, "motions": mapped, "sampleBatches": [{"file": samples.name, "sha256": digest(samples)}]}
        write(project / TUTORIAL_RECEIPT, profile)
        publish_directory(project, output)
    return profile


def project(room, catalog, controller, chapters, source_receipt, models):
    source = load(source_receipt)
    baked = load(models / TUTORIAL_RECEIPT)
    if (source["controllerIndexSHA256"] != digest(controller) or source["catalogSHA256"] != digest(catalog)
            or baked["sourceReceiptSHA256"] != digest(source_receipt)):
        raise ValueError("Tutorial camera requires the locked controller and sampled models")
    body, main, _ = inputs(room, catalog, controller, chapters)
    commands, = (chapter["commands"] for chapter in load(chapters)["chapters"] if chapter["group"] == "tutorial_01")
    directions = [(row["row"], row["command"], row.get("bodyMotion")) for row in commands
                  if row["command"] in ("DirectionGameStartCamera", "ChangeMotion")]
    if directions != [(16, "DirectionGameStartCamera", None), (26, "ChangeMotion", 401)]:
        raise ValueError("Expected the ordered original camera and end commands")
    durations = {**load(room / "opening-body-receipt.json")["motions"], **baked["motions"]}
    controller_data = load(controller)
    for name, value in baked["motions"].items():
        row, = (asset for path, asset in source["cameraClips"].items() if Path(path).stem == name)
        if row["sha256"] != value["sourceSHA256"]:
            raise ValueError(f"Camera sample uses a different clip: {name}")
        states = {state["m_Name"] for state in controller_data["states"].values()
                  if state["m_Motion"].get("guid") and any(Path(path).stem == name for path in
                       controller_data["externalAssets"].get(state["m_Motion"]["guid"], ()))}
        for state in sorted(states):
            body["states"].append({"name": "story-" + state, "clip": value["clipIndex"], "loop": row["loop"]})
    graph, tutorial, bindings = compile_signals(body, controller_data, load(catalog), durations, source["cameraClips"])
    script = integration((room / "assets/pomodoro.lua").read_text(encoding="utf-8"), main, tutorial)
    return graph, script, tutorial, bindings


def stage(room, catalog, controller, chapters, source_receipt, models, output, camera_trace=None,
          camera_audio_source=None, face_catalog=None, achievements=False, blackout_source_lock=None, image_catalog=None,
          window_settings=False, window_settings_inventory=None, chromatic_source_lock=None, screen_move_source_lock=None):
    body, program, route, states = project(room, catalog, controller, chapters, source_receipt, models)
    display_rows = None
    if window_settings_inventory:
        display_rows = display_localization(room, window_settings_inventory)
        program = bind_display_language(program)
        window_settings = True
    camera = load(models / TUTORIAL_RECEIPT)
    camera_path = None
    if camera_audio_source and not camera_trace:
        raise ValueError("Camera audio requires the original camera trace")
    if camera_trace:
        program, camera_path = camera_project(room, program, camera_trace, camera_audio_source)
    if face_catalog:
        program = face_program(program, load(face_catalog), models, chapters)
    if bool(blackout_source_lock or chromatic_source_lock or screen_move_source_lock) != bool(image_catalog):
        raise ValueError("Opening effects require a source lock and the room image catalog")
    effects = {}
    if blackout_source_lock:
        program = bind_blackout(program)
        effects["blackout"] = blackout_inputs(blackout_source_lock, image_catalog)
    if chromatic_source_lock:
        program = bind_chromatic(program)
        effects["chromatic"] = chromatic_inputs(chromatic_source_lock, image_catalog)
    if screen_move_source_lock:
        program = bind_screen_move(program)
        effects["screen-move"] = screen_move_inputs(screen_move_source_lock, image_catalog)
    scripts = achievement_programs(program) if achievements else {"pomodoro.lua": program}
    with tempfile.TemporaryDirectory(prefix="tutorial-camera-room-", dir=output.parent) as workspace:
        target = Path(workspace) / "project"
        assets = target / "assets"
        shutil.copytree(room / "assets", assets, copy_function=copy_room_asset, ignore=skip_capture_orphans)
        for path in [room / "project.azureproject", *room.glob("*-receipt.json")]:
            shutil.copyfile(path, target / path.name)
        for part, expected in camera["parts"].items():
            model = models / "assets/heroine" / (part + ".glb")
            if digest(model) != expected["sha256"]:
                raise ValueError(f"Camera actor model changed: {part}")
            staged = assets / "heroine" / model.name
            staged.unlink()
            os.link(model, staged)
        write(assets / GRAPH, body)
        script = assets / "pomodoro.lua"
        for name, content in scripts.items():
            (assets / name).write_text(content, encoding="utf-8", newline="\n")
        level = assets / "room.azurelevel"
        display = None
        if camera_audio_source or effects or window_settings:
            scene = load(level)
            if window_settings:
                descriptor = target / "project.azureproject"
                project_configuration = load(descriptor)
                display = install_window_settings(project_configuration, scene,
                    (assets / "timer.rml").read_text(encoding="utf-8"), localization=display_rows)
                write(descriptor, project_configuration)
                for name, content in display.items():
                    (assets / name).write_text(content, encoding="utf-8", newline="\n")
            for name, (node, media, _) in effects.items():
                if any(value["id"] == node["id"] for value in scene["nodes"]):
                    raise ValueError(f"Room already has an opening effect: {name}")
                scene["nodes"].append(node)
                for name, path in media.items():
                    os.link(path, assets / name)
            if camera_audio_source:
                sound = camera_path["cue"]
                attach_audio(scene, sound)
                stage_live_audio_media(camera_audio_source, [sound], assets)
            write(level, scene)
        for name in ("opening-desk-receipt.json", "opening-story-room-receipt.json", "opening-story-motion-receipt.json"):
            manifest = load(target / name)
            manifest["scriptSHA256"] = digest(script)
            if (camera_audio_source or effects or display) and "levelSHA256" in manifest:
                manifest["levelSHA256"] = digest(level)
            if display and "documentSHA256" in manifest:
                manifest["documentSHA256"] = digest(assets / "timer.rml")
            if name == "opening-story-motion-receipt.json":
                manifest.update(graphSHA256=digest(assets / GRAPH), tutorialCamera=route, tutorialStates=states)
            write(target / name, manifest)
        for name in ("opening-body-receipt.json", "timer-motion-receipt.json"):
            path = target / name
            if path.exists():
                manifest = load(path)
                for part, expected in camera["parts"].items():
                    manifest["parts"][part]["sha256"] = expected["sha256"]
                write(path, manifest)
        write(target / TUTORIAL_RECEIPT, camera)
        morphology = models / MORPH_RECEIPT
        if morphology.is_file():
            if load(morphology)["faceGlbSHA256"] != camera["parts"]["face"]["sha256"]:
                raise ValueError("Face asset does not match its body binding receipt")
            shutil.copyfile(morphology, target / MORPH_RECEIPT)
        if face_catalog:
            write(target / "opening-face-layer-receipt.json", {
                "schemaVersion": 1, "catalogSHA256": digest(face_catalog), "scriptSHA256": digest(script),
                "faceGlbSHA256": camera["parts"]["face"]["sha256"]})
        if achievements:
            write(target / "room-achievement-events-receipt.json", {
                "schemaVersion": 1, "controllers": {name: digest(assets / name) for name in scripts},
                "mode": "local-room-events"})
        for name, (_, _, effect_receipt) in effects.items():
            write(target / f"opening-{name}-receipt.json", effect_receipt | {
                "scriptSHA256": digest(script), "levelSHA256": digest(level)})
        if display:
            display_receipt = {
                "schemaVersion": 1, "assets": {name: digest(assets / name) for name in display},
                "levelSHA256": digest(level), "projectSHA256": digest(target / "project.azureproject")}
            if window_settings_inventory:
                display_receipt.update(inventorySHA256=digest(window_settings_inventory), timerSHA256=digest(script))
            write(target / "window-settings-receipt.json", display_receipt)
        if camera_path:
            write(target / "opening-camera-receipt.json", camera_path | {
                "scriptSHA256": digest(script), "levelSHA256": digest(level)})
        publish_directory(target, output)
    return camera


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    steps = parser.add_subparsers(dest="step", required=True)
    bake_args = ("room", "rig", "source-receipt", "samples", "output")
    room_args = ("room", "catalog", "controller", "chapters", "source-receipt", "models", "output")
    for step, names in (("bake", bake_args), ("stage", room_args)):
        action = steps.add_parser(step)
        for name in names:
            action.add_argument("--" + name, type=Path, required=True)
    stage_options = ("camera-trace", "camera-audio-source", "face-catalog", "blackout-source-lock",
                     "chromatic-source-lock", "screen-move-source-lock", "image-catalog", "window-settings-inventory")
    for name in stage_options:
        steps.choices["stage"].add_argument("--" + name, type=Path)
    steps.choices["stage"].add_argument("--achievements", action="store_true")
    steps.choices["stage"].add_argument("--window-settings", action="store_true")
    args = vars(parser.parse_args())
    arguments = bake_args if args["step"] == "bake" else room_args
    handler = bake if args["step"] == "bake" else stage
    options = {name: args[name].resolve(strict=True) for name in (option.replace("-", "_") for option in stage_options)
               if args.get(name)}
    if args.get("achievements"):
        options["achievements"] = True
    if args.get("window_settings"):
        options["window_settings"] = True
    print(json.dumps(handler(*(args[name.replace("-", "_")].resolve() for name in arguments), **options)))
