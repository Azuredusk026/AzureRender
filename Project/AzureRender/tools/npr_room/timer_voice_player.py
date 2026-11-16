"""Assemble the timer speech and body cues with the localized room."""

import argparse
import json
from pathlib import Path
import shutil
import tempfile

from tools.npr_room.build_voice_room import localized_text_config
from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest
from tools.npr_room.room_geometry import copy_room_asset, stage_live_audio_media
from tools.npr_room.room_language import FIELDS, localized_scripts


def program_text(catalog, motions, cue_replay):
    output = ["local timer_voice_program = {", "    rules = {"]
    for name, row in sorted(catalog["selectors"].items()):
        fields = ", ".join(f"{key} = {value}" for key, value in sorted(row.items()))
        output.append(f"        {name} = {{{fields}}},")
    output.extend(("    },", "    motions = {"))
    for identity, motion in sorted(motions.items()):
        cues = ", ".join(f"[{cue}] = {seconds}" for cue, seconds in motion["cues"].items())
        output.append(f'        [{identity}] = {{state = "{motion["state"]}", seconds = {motion["seconds"]}, cues = {{{cues}}}}},')
    output.extend(("    },", "    groups = {"))
    for category, rows in sorted(catalog["groups"].items()):
        output.append(f"        [{category}] = {{")
        for row in rows:
            asset = json.dumps("assets:/" + row["asset"])
            key = json.dumps(row["localization"])
            text = localized_text_config(row["translations"], FIELDS)
            before = str(row["speakBeforeMotion"]).lower()
            output.append(f"            {{asset = {asset}, localization = {key}, text = {text}, "
                          f"seconds = {row['frames']} / {row['sampleRate']}, bodyMotion = {row['bodyMotion']}, "
                          f"animationCue = {row['animationCue']}, speakBeforeMotion = {before}}},")
        output.append("        },")
    output.append("    },")
    if cue_replay:
        output.append("    random = function(limit) return limit or .95 end,")
    output.append("}")
    return ("\n".join(output) + "\n").encode("utf-8")


def stage_timer_voice(room, output, catalog_path, source, cue_replay=False):
    catalog = json.loads(catalog_path.read_bytes())
    motion_receipt = json.loads((room / "timer-motion-receipt.json").read_bytes())
    language = json.loads((room / "language-receipt.json").read_bytes())
    voice = json.loads((room / "voice-receipt.json").read_bytes())
    work = json.loads((room / "choice-receipt.json").read_bytes())
    rewards = json.loads((room / "work-progression-receipt.json").read_bytes())
    motions = motion_receipt["motions"]
    if motion_receipt["sourceSHA256"] != digest(catalog_path):
        raise ValueError("Timer body motions and speech must share an original catalog")
    if any(item["storySHA256"] != catalog["storySHA256"] for item in (language, voice, work)):
        raise ValueError("Timer speech must share the room's original story inventory")
    if motions.keys() != catalog["motions"].keys() and not cue_replay:
        raise ValueError("Automatic speech requires all source animation routes")
    if cue_replay and not {"55", "54", "69"} <= motions.keys():
        raise ValueError("Timer cue replay requires work, break, and finish motions")
    example = Path(__file__).resolve().parents[2] / "examples/npr-room/assets"
    with tempfile.TemporaryDirectory(prefix="timer-voice-player-", dir=output.parent) as container:
        project = Path(container) / "project"
        assets = project / "assets"
        shutil.copytree(room / "assets", assets, copy_function=copy_room_asset, ignore=shutil.ignore_patterns(".azure"))
        for item in room.glob("*"):
            if item.is_file() and (item.name == "project.azureproject" or item.name.endswith("-receipt.json")):
                shutil.copyfile(item, project / item.name)
        missing = []
        for audio in catalog["media"]:
            target = assets / audio["asset"]
            if not target.is_file():
                missing.append(audio)
            elif digest(target) != audio["sha256"]:
                raise ValueError(f"Shared voice source differs: {audio['guid']}")
        stage_live_audio_media(source, missing, assets)
        level_path = assets / "room.azurelevel"
        level = json.loads(level_path.read_bytes())
        if any(node["id"] == "room-timer-voice" for node in level["nodes"]):
            raise ValueError("Room already has a timer voice source")
        level["nodes"].append({"id": "room-timer-voice", "components": {
            "azure.audio-source": {"type": "azure.audio-source", "version": 1, "data": {"volume": .5}},
        }})
        write(level_path, level)
        ui_path = assets / "timer.rml"
        rml = ui_path.read_text(encoding="utf-8")
        style, body = (example / "timer-voice.rml").read_text(encoding="utf-8").split("</style>\n", 1)
        ui_path.write_text(rml.replace("</style>", style + "</style>", 1).replace("</body>", body + "</body>", 1),
                           encoding="utf-8", newline="\n")
        scripts = [program_text(catalog, motions, cue_replay),
                   *((example / file).read_bytes() for file in ("timer-voice-selector.lua", "timer-voice-action.lua")),
                   *localized_scripts(example, language["ui"], language["default"], voice, work, rewards)]
        (assets / "pomodoro.lua").write_bytes(b"\n".join(scripts))
        receipt = {"schemaVersion": 1, "catalogSHA256": digest(catalog_path), "cueReplay": cue_replay,
                   "motions": motions, "selectedVoices": {
                       event: catalog["groups"][group][0] for event, group in
                       (("work", "4"), ("break", "5"), ("finish", "6"))} if cue_replay else {},
                   "availableAudioSources": len(catalog["media"])}
        write(project / "timer-voice-receipt.json", receipt)
        publish_directory(project, output)
    return receipt


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("room", "output", "catalog", "source"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--cue-replay", action="store_true")
    args = parser.parse_args()
    receipt = stage_timer_voice(args.room, args.output, args.catalog, args.source, args.cue_replay)
    print(json.dumps({"motions": len(receipt["motions"]), "voiceSources": receipt["availableAudioSources"],
                      "cueReplay": receipt["cueReplay"]}))
