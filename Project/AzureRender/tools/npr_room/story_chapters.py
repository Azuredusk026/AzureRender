"""Compile the original room tutorial and first main chapter into room story data."""

import argparse
from collections import Counter
import json
import math
from pathlib import Path

from tools.npr_room.build_voice_room import LANGUAGES, lua_string, voice_source_index, voiced_audio
from tools.npr_room.reference_inventory import digest
from tools.npr_room.story_routes import COMMAND_ENUM, SCENARIO_READER, command_names


OPENING_CHAPTERS = ("tutorial_01", "main_01")
BRANCHES = {"Jump", "Selection", "TutorialSelection"}
COMMANDS = BRANCHES | {"Text", "WaitForSeconds", "PlayBGM", "ChangeWindowView", "EndScenario", "StartSelectOnce",
                      "TutorialFocusPomodoro", "TutorialFocusTodo", "TutorialFocusNote", "ChangeMotion",
                      "TalkLogOnlyPlayBGM", "ChangeLayout", "ReadNextForScript", "DeactivateNormalText",
                      "SetDirectionDelaySeconds", "DirectionGlitch1", "DirectionScreenMove", "DirectionBlackOut",
                      "DirectionMonochromeNoise", "DirectionChromaticAberration", "VoiceDefectRatio",
                      "DirectionGameStartCamera"}


def translations(row):
    return {language: row[language] for language in LANGUAGES}


def timing(value, row):
    number = float(value or 0)
    if not math.isfinite(number) or number < 0:
        raise ValueError(f"Invalid non-negative story timing at row {row}")
    return number


def compile_chapters(story, names, audio_catalog, native_audio, selected):
    groups = {row["ID"]: row for row in story["groups"]}
    localization = {row["ID"]: row for row in story["localization"]}
    voice_index = voice_source_index(audio_catalog)
    chapters, media = [], {}
    if not selected or len(set(selected)) != len(selected):
        raise ValueError("Select a unique set of story chapters")
    for identity in selected:
        group = groups[identity]
        positions = story["commandsByGroup"][identity]
        source = story["commands"]
        offsets, commands = {}, []
        if not positions or len(set(positions)) != len(positions):
            raise ValueError(f"Expected original ordered story rows for {identity}")
        for index, position in enumerate(positions, 1):
            if source[position]["ScenarioGroupID"] != identity:
                raise ValueError(f"Story row {position} belongs to a different chapter")
            offsets.setdefault(source[position]["ID"], index)
        for position in positions:
            row = source[position]
            command = names.get(int(row["Command"]))
            if command not in COMMANDS:
                raise ValueError(f"Unsupported opening story command at row {position}: {row['Command']}")
            arguments = [row[f"Arg{index}"] for index in range(1, 5)]
            compiled = {"row": position, "id": row["ID"], "command": command, "arguments": arguments}
            if command in BRANCHES:
                if arguments[0] not in offsets:
                    raise ValueError(f"Unknown local story target at row {position}: {arguments[0]}")
                compiled["target"] = offsets[arguments[0]]
            if command in {"Selection", "TutorialSelection", "Text"}:
                localized = localization[row["LocalizationID"]]
                compiled.update(localization=localized["ID"], translations=translations(localized))
            if command == "TutorialSelection":
                compiled["usedKey"] = int(arguments[1])
            if command in {"Text", "ChangeMotion"}:
                compiled.update(bodyMotion=int(row["BodyMotion"]), faceMotion=int(row["FacialMotion"]),
                                look={"scale": float(row["LookScale"]), "seconds": timing(row["LookSpeedSeconds"], position),
                                      "ease": int(row["LookEaseType"])})
            if command == "Text":
                compiled.update(talker=int(row["Talker"]), delay=timing(arguments[0], position),
                                hold=timing(arguments[1], position), moveMouth=arguments[2] == "0")
                if localized["Voice"]:
                    voice, audio = voiced_audio(localized, voice_index, native_audio, f"story row {position}")
                    compiled.update(voice)
                    media[audio["guid"]] = audio
            if command == "WaitForSeconds":
                compiled["seconds"] = timing(arguments[0], position)
            commands.append(compiled)
        category, episode, level = (int(group[key]) for key in ("Scenario", "EpisodeNumber", "UnlockLevel"))
        if category not in {1, 18} or episode < 1 or level < 0:
            raise ValueError(f"Unexpected opening story progression: {identity}")
        chapters.append({"group": identity, "category": category, "episode": episode, "unlockLevel": level,
                         "title": translations(localization[group["TitleLocalizationID"]]),
                         "subtitle": translations(localization[group["SubtitleLocalizationID"]]), "commands": commands})
    return chapters, list(media.values())


def extract(source, inventory, audio_catalog, audio_probe):
    command_enum, reader = source / COMMAND_ENUM, source / SCENARIO_READER
    chapters, media = compile_chapters(json.loads(inventory.read_bytes()),
        command_names(command_enum.read_text(encoding="utf-8-sig")), json.loads(audio_catalog.read_bytes())["slots"],
        json.loads(audio_probe.read_bytes())["files"], OPENING_CHAPTERS)
    commands = Counter(row["command"] for chapter in chapters for row in chapter["commands"])
    return {"schemaVersion": 1, "sourceSHA256": {"inventory": digest(inventory), "commandEnum": digest(command_enum),
              "reader": digest(reader), "audioCatalog": digest(audio_catalog), "audioProbe": digest(audio_probe)},
            "chapters": chapters, "media": media, "commandCounts": dict(commands)}


def lua_value(value):
    if isinstance(value, str):
        return lua_string(value)
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, (int, float)) and math.isfinite(value):
        return str(value)
    if isinstance(value, list):
        return "{" + ",".join(lua_value(item) for item in value) + "}"
    if isinstance(value, dict):
        return "{" + ",".join(f"[{lua_string(key)}]={lua_value(item)}" for key, item in value.items()) + "}"
    raise ValueError(f"Unsupported story program value: {value!r}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("source", "inventory", "audio-catalog", "audio-probe", "output"):
        parser.add_argument("--" + argument, type=Path, required=True)
    parser.add_argument("--lua-output", type=Path)
    args = parser.parse_args()
    data = extract(args.source, args.inventory, args.audio_catalog, args.audio_probe)
    with args.output.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(data, stream, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
        stream.write("\n")
    if args.lua_output:
        with args.lua_output.open("x", encoding="utf-8", newline="\n") as stream:
            stream.write("local room_story_chapters = " + lua_value(data["chapters"]) + "\n")
    print(json.dumps({"chapters": len(data["chapters"]), "media": len(data["media"]), "commands": data["commandCounts"]}))
