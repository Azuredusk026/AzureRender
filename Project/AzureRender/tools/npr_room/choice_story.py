"""Read the original work conversation commands and group-local options."""

from collections import Counter
import math

from tools.npr_room.build_voice_room import LANGUAGES, spoken_line, voice_source_index
from tools.npr_room.build_work_calendar import PARAMETER_ASSET
from tools.npr_room.reference_inventory import digest
from tools.npr_room.skinned_mesh_export import UnityScene


SHORT_CONVERSATION = "7"
TEXT = "0"
OPTION = "3"
END = "12"


def choice_episodes(story):
    groups = sorted((row for row in story["groups"] if row["Scenario"] == SHORT_CONVERSATION),
                    key=lambda row: int(row["EpisodeNumber"]))
    commands = story["commands"]
    episodes = []
    if [int(row["EpisodeNumber"]) for row in groups] != list(range(1, len(groups) + 1)) or not groups:
        raise ValueError("Invalid work conversation episode sequence")
    for group in groups:
        group_id = group["ID"]
        rows = story["commandsByGroup"][group_id]
        kinds = [commands[row]["Command"] for row in rows]
        if len(rows) < 2 or kinds[0] != TEXT or kinds[1] != OPTION:
            raise ValueError(f"Invalid conversation introduction in {group_id}")
        positions = {}
        for offset, row in enumerate(rows):
            positions.setdefault(commands[row]["ID"], offset)
        offsets = []
        for offset in range(1, len(rows)):
            if kinds[offset] != OPTION:
                break
            offsets.append(offset)
        options = []
        for offset in offsets:
            label = rows[offset]
            target = positions.get(commands[label]["Arg1"])
            if target is None or target + 1 >= len(rows) or kinds[target:target + 2] != [TEXT, END]:
                raise ValueError(f"Missing spoken response and terminal command for option row {label}")
            options.append({"labelRow": label, "spokenRow": rows[target], "endRow": rows[target + 1]})
        covered = [rows[0]]
        for option in options:
            covered.extend(option.values())
        if Counter(rows) != Counter(covered):
            raise ValueError(f"Conversation commands are not covered by their option routes: {group_id}")
        episodes.append({"group": group_id, "episode": int(group["EpisodeNumber"]),
                         "intro": rows[0], "options": options})
    return episodes


def choice_parameters(source):
    asset = source / PARAMETER_ASSET
    settings = UnityScene(asset).read(11400000)["PomodoroTalkData"]
    probability = settings["maxTalkProbability"]
    work_seconds = settings["maxWorkedMinutes"] * 60
    available_seconds = settings["_pomodoroChoiceTalkAvailableTimeMinutes"] * 60
    if not 0 <= probability <= 1 or any(not math.isfinite(value) or value <= 0
                                        for value in (work_seconds, available_seconds)):
        raise ValueError("Invalid work conversation probability parameters")
    return {"probability": probability, "fullProbabilitySeconds": work_seconds,
            "availableSeconds": available_seconds, "sourceSHA256": digest(asset)}


def choice_program(story, catalog, native):
    sources = voice_source_index(catalog)
    localizations = {row["ID"]: row for row in story["localization"]}
    commands = story["commands"]
    voices = {}
    program = []

    def spoken(row_index):
        row = commands[row_index]
        line, media = spoken_line(row, localizations[row["LocalizationID"]], sources, native, f"row {row_index}")
        voices[media["guid"]] = media
        return {"row": row_index, **line}

    for episode in choice_episodes(story):
        options = []
        for option in episode["options"]:
            label = localizations[commands[option["labelRow"]]["LocalizationID"]]
            options.append({"labelRow": option["labelRow"], "label": {language: label[language] for language in LANGUAGES},
                            "response": spoken(option["spokenRow"]), "endRow": option["endRow"]})
        program.append({"group": episode["group"], "episode": episode["episode"],
                        "intro": spoken(episode["intro"]), "options": options})
    return program, list(voices.values())
