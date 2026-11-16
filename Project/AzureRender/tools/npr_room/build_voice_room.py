"""Stage original idle character lines with localized text and exact voice clips."""

import argparse
from collections import defaultdict
import json
import math
from pathlib import Path
import re
import shutil
import tempfile
from uuid import UUID

from tools.npr_room.build_work_calendar import calendar_script, work_rewards
from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest
from tools.npr_room.room_geometry import copy_room_asset, stage_live_audio_media


LANGUAGES = ("ZhHans", "ZhHant", "Ja", "En", "Ko", "Pt", "Ru")


def voice_source_index(catalog):
    sources = defaultdict(list)
    for guid, row in catalog.items():
        path = Path(row["source"])
        if path.suffix.lower() == ".ogg":
            sources[path.stem].append((guid, row))
    return sources


def spoken_line(row, localized, sources, native, context):
    voice = localized["Voice"]
    matches = sources[voice]
    if len(matches) != 1:
        raise ValueError(f"Expected one original voice source for {context}: {voice}")
    guid, clip = matches[0]
    uuid = str(UUID(guid))
    asset = f"voice/{uuid}.ogg"
    media = {"source": clip["source"], "sha256": clip["sha256"], "guid": guid, "uuid": uuid, "asset": asset}
    sample = native[clip["sha256"] + ".ogg"]
    frames, rate = sample["frames"], sample["sampleRate"]
    delay, hold = float(row["Arg1"]), float(row["Arg2"])
    if frames <= 0 or rate <= 0 or not all(math.isfinite(value) and value >= 0 for value in (delay, hold)):
        raise ValueError(f"Invalid spoken line timing in {context}")
    return {"localization": row["LocalizationID"], "voice": voice, "audioGuid": guid, "asset": asset,
            "frames": frames, "sampleRate": rate, "delay": delay, "hold": hold,
            "bodyMotion": int(row["BodyMotion"]), "faceMotion": int(row["FacialMotion"]),
            "moveMouth": row["Arg3"] == "0",
            "translations": {language: localized[language] for language in LANGUAGES}}, media


def idle_responses(story, catalog, native):
    sources = voice_source_index(catalog)
    translations = {row["ID"]: row for row in story["localization"]}
    groups = sorted((row for row in story["groups"] if row["Scenario"] == "8"),
                    key=lambda row: int(row["EpisodeNumber"]))
    responses, media = [], {}
    if [int(row["EpisodeNumber"]) for row in groups] != list(range(1, len(groups) + 1)) or not groups:
        raise ValueError("Idle response episode sequence is invalid")
    for group in groups:
        group_id = group["ID"]
        rows = story["commandsByGroup"][group_id]
        if len(rows) != 1 or story["commands"][rows[0]]["Command"] != "0":
            raise ValueError(f"Expected one idle response command in {group_id}")
        row = story["commands"][rows[0]]
        localized = translations[row["LocalizationID"]]
        line, original = spoken_line(row, localized, sources, native, group_id)
        responses.append({"group": group_id, "episode": int(group["EpisodeNumber"]), **line})
        media[original["guid"]] = original
    return responses, list(media.values())


def localized_text_config(translations, language):
    def quote(value):
        escaped = value.replace("\\", "\\\\").replace('"', '\\"')
        return '"' + re.sub(r"[\x00-\x1f]", lambda match: f"\\{ord(match[0]):03d}", escaped) + '"'

    if isinstance(language, str):
        return quote(translations[language])
    return "{" + ", ".join(quote(translations[field]) for field in language) + "}"


def voice_line_config(row, language):
    asset = json.dumps("assets:/" + row["asset"])
    text = localized_text_config(row["translations"], language)
    return f"{{{asset}, {text}, {row['frames']} / {row['sampleRate']}, {row['delay']}, {row['hold']}}}"


def voice_config(responses, language):
    lines = [f"    {voice_line_config(row, language)}," for row in responses]
    return "local responses = {\n" + "\n".join(lines) + "\n}\n"


def stage_voice(room, output, source, inventory, audio_catalog, audio_probe, language):
    example = Path(__file__).resolve().parents[2] / "examples/npr-room/assets"
    responses, media = idle_responses(json.loads(inventory.read_bytes()),
                                     json.loads(audio_catalog.read_bytes())["slots"],
                                     json.loads(audio_probe.read_bytes())["files"])
    descriptor = json.loads((room / "project.azureproject").read_bytes())
    descriptor["runtime"]["input"]["room-voice-talk"] = []
    rewards = work_rewards(source)
    with tempfile.TemporaryDirectory(prefix="room-voice-", dir=output.parent) as parent:
        project = Path(parent) / "project"
        assets = project / "assets"
        shutil.copytree(room / "assets", assets, copy_function=copy_room_asset, ignore=shutil.ignore_patterns(".azure"))
        level_path, ui_path, timer = assets / "room.azurelevel", assets / "timer.rml", assets / "pomodoro.lua"
        level = json.loads(level_path.read_bytes())
        if any(node["id"] == "room-voice" for node in level["nodes"]):
            raise ValueError("Room already has a voice source")
        level["nodes"].append({"id": "room-voice", "components": {
            "azure.audio-source": {"type": "azure.audio-source", "version": 1, "data": {"volume": .5}},
        }})
        ui = ui_path.read_text(encoding="utf-8")
        style, body = (example / "room-voice.rml").read_text(encoding="utf-8").split("</style>\n", 1)
        ui = ui.replace("</style>", style + "</style>", 1).replace("</body>", body + "</body>", 1)
        timer_sources = [voice_config(responses, language).encode("utf-8"),
                         (example / "room-voice.lua").read_bytes(), calendar_script(example, rewards)]
        stage_live_audio_media(source, media, assets)
        write(project / "project.azureproject", descriptor)
        write(project / "voice-receipt.json", {"schemaVersion": 1, "storySHA256": digest(inventory), "language": language,
                                               "responses": responses, "media": media})
        write(project / "work-progression-receipt.json", rewards)
        for receipt in ("music-receipt.json", "ambient-receipt.json"):
            shutil.copyfile(room / receipt, project / receipt)
        write(level_path, level)
        ui_path.write_text(ui, encoding="utf-8", newline="\n")
        timer.write_bytes(b"\n".join(timer_sources))
        publish_directory(project, output)
    return {"responses": len(responses), "voiceSources": len(media), "languages": list(LANGUAGES)}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("room", "source", "output", "inventory", "audio-catalog", "audio-probe"):
        parser.add_argument("--" + argument, type=Path, required=True)
    parser.add_argument("--language", choices=LANGUAGES, default="ZhHans")
    args = parser.parse_args()
    print(json.dumps(stage_voice(args.room, args.output, args.source, args.inventory, args.audio_catalog,
                                 args.audio_probe, args.language)))
