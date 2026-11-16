"""Assemble the local P1 probe from an imported desktop furniture project."""

import argparse
import json
import os
from pathlib import Path
import shutil

from reference_inventory import asset_index, digest, group_paths_by_guid
from room_geometry import NOTE_ACTIONS, TODO_ACTIONS, activate_room_control, ui_font_document
from room_geometry import install_heroine_motion, install_room_geometry, install_todo_audio, merge_room_geometry


CUES = {"timer-start": (0, 0.5), "timer-pause": (6, 1.0), "timer-finish": (3, 0.5)}
SYSTEM_CUES = {"settings-click": "bulbul_systemse_click.wav", "settings-cancel": "bulbul_systemse_cancel.wav"}


def component(type_id, data):
    return {"type": type_id, "version": 1, "data": data}


def add_startup_flow(output):
    project = output / "project.azureproject"
    descriptor = json.loads(project.read_text(encoding="utf-8"))
    descriptor["runtime"]["input"].update({"transition-cancel": [259], "transition-retry": [257]})
    project.write_text(json.dumps(descriptor, indent=2) + "\n", encoding="utf-8")
    assets = output / "assets"
    for stage in ("boot", "asset-load", "entry", "transition"):
        components = {"azure.script": component("azure.script", {"asset": "assets:/startup-flow.lua"})}
        if stage == "transition":
            components["azure.game-ui"] = component("azure.game-ui", {"asset": "assets:/startup-flow.rml"})
        level = {
            "schemaVersion": 1, "id": stage, "sceneType": "sample", "resources": [], "lights": [],
            "nodes": [{"id": "startup" if stage == "boot" else stage, "components": components}],
        }
        (assets / f"{stage}.azurelevel").write_text(json.dumps(level, indent=2) + "\n", encoding="utf-8")


def make_probe(example, source, furniture, output, audio_playback=None, geometry=None, heroine=None, notes=False,
               ui_font=None, todo=False):
    import re

    master = source / "Assets/App/BulbulAssets/MasterData/ScriptableObjects/TimerSeMaster.asset"
    cues = dict(re.findall(r"(?m)^  - SeSound: (\d+)\n    AudioClip: \{fileID: \d+, guid: ([a-f0-9]{32}),", master.read_text(encoding="utf-8")))
    by_guid = group_paths_by_guid(asset_index(source))
    audio = {}
    for slot, (cue, volume) in CUES.items():
        relative, = by_guid[cues[str(cue)]]
        original = source / relative
        if original.suffix.lower() != ".ogg":
            raise ValueError(f"Expected OGG for {slot}")
        audio[slot] = (original, volume)
    if audio_playback is not None:
        playback = audio_playback.resolve(strict=True)
        repairs = json.loads((playback / "audio-repairs.json").read_text(encoding="utf-8"))["repairs"]
        for slot, name in SYSTEM_CUES.items():
            original = source / "Assets/App/BulbulAssets/Audio/SE" / name
            sha = digest(original)
            entry = repairs[f"media/{sha}.wav"]
            corrected = (playback / entry["media"]).resolve(strict=True)
            if corrected.parent != playback or digest(corrected) != entry["sha256"] or entry["sourceSHA256"] != sha:
                raise ValueError(f"Invalid playback media for {slot}")
            audio[slot] = (corrected, 1.0)

    level = json.loads((furniture / "assets/furniture.azurelevel").read_text(encoding="utf-8"))
    import_record = level["sceneImports"]["recovery-furniture"]
    chair_id = import_record["nodeMap"]["transform-7570"]
    chair, = [node for node in level["nodes"] if node["id"] == chair_id]
    chair["components"]["azure.animator"] = component(
        "azure.animator", {"asset": "assets:/chair-motion.json", "state": "window"}
    )
    level["id"] = "room"
    level["nodes"].append({
        "id": "room-timer",
        "components": {
            "azure.script": component("azure.script", {"asset": "assets:/pomodoro.lua", "enabled": True}),
            "azure.game-ui": component("azure.game-ui", {"asset": "assets:/timer.rml", "enabled": True}),
        },
    })
    for slot, (original, volume) in audio.items():
        level["nodes"].append({
            "id": slot,
            "components": {
                "azure.audio-source": component(
                    "azure.audio-source", {"asset": f"assets:/{slot}{original.suffix}", "volume": volume},
                ),
            },
        })

    if geometry is not None:
        imported_room = json.loads((geometry / "checks/room-camera.azurelevel").read_bytes())
        level, replacements = merge_room_geometry(level, imported_room)

    output.mkdir()
    project = output / "project.azureproject"
    shutil.copy2(example / "project.azureproject", project)
    if notes or todo:
        descriptor = json.loads(project.read_bytes())
        if notes:
            activate_room_control(descriptor, level, "notes", NOTE_ACTIONS)
        if todo:
            activate_room_control(descriptor, level, "todo", TODO_ACTIONS)
        project.write_text(json.dumps(descriptor, indent=2) + "\n", encoding="utf-8")
    assets = output / "assets"
    shutil.copytree(example / "assets", assets)
    if todo and audio_playback is not None:
        install_todo_audio(source, audio_playback, assets, level)
    if ui_font is not None:
        if ui_font.suffix.lower() not in (".otf", ".ttf"):
            raise ValueError("The UI fallback requires an OpenType or TrueType font")
        filename = "ui-fallback" + ui_font.suffix.lower()
        os.link(ui_font, assets / filename)
        rml = assets / "timer.rml"
        rml.write_text(ui_font_document(rml.read_text(encoding="utf-8"), filename), encoding="utf-8")
    if geometry is None:
        shutil.copytree(furniture / "assets/exchange", assets / "exchange",
                        ignore=shutil.ignore_patterns("dangling.azuresceneexchange*"))
    else:
        install_room_geometry(geometry, furniture / "assets/exchange", assets, replacements)
    for slot, (original, _) in audio.items():
        shutil.copyfile(original, assets / f"{slot}{original.suffix}")
    if heroine is not None:
        install_heroine_motion(heroine, assets, level)
    (assets / "room.azurelevel").write_text(json.dumps(level, indent=2) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--furniture", type=Path, required=True)
    parser.add_argument("--audio-playback", type=Path)
    parser.add_argument("--room-geometry", type=Path, help="Imported room project with room-camera.azurelevel")
    parser.add_argument("--heroine-motion", type=Path, help="Verified project with sampled heroine motions")
    parser.add_argument("--notes", action="store_true")
    parser.add_argument("--todo", action="store_true")
    parser.add_argument("--ui-font", type=Path, help="Source font used for glyph fallback in the room UI")
    parser.add_argument("--startup-flow", action="store_true")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    example = Path(__file__).resolve().parents[2] / "examples/npr-room"
    make_probe(example, args.source, args.furniture, args.output, args.audio_playback, args.room_geometry,
               args.heroine_motion, args.notes, args.ui_font, args.todo)
    if args.startup_flow:
        add_startup_flow(args.output)


if __name__ == "__main__":
    main()
