"""Join locked Recovery feature catalogs without treating source rows as runtime coverage."""

import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import re


def record(entry, preconditions, inputs, expected, persistence, source, engine_gap, evidence=None):
    return {"entry": entry, "preconditions": preconditions, "input": inputs, "expected": expected,
            "persistence": persistence, "source": source, "engineGap": engine_gap,
            "evidence": evidence or []}


def enum_values(text):
    result = {}
    declared = {}
    for name, value in re.findall(r"^\s*(\w+)\s*=\s*(\w+)\s*,?\s*$", text, re.MULTILINE):
        number = int(value) if value.isdecimal() else declared[value]
        if name in declared:
            raise ValueError(f"Duplicate enum name: {name}")
        declared[name] = number
        result.setdefault(number, name)
    if not result:
        raise ValueError("Missing enum values")
    return result


def story_rows(inventory, routes, scenario_types):
    groups = {group["ID"]: group for group in inventory["groups"]}
    by_group = inventory["commandsByGroup"]
    if len(groups) != len(inventory["groups"]) or by_group.keys() != groups.keys():
        raise ValueError("Story groups and command index differ")
    commands = inventory["commands"]
    partition = Counter()
    for group, positions in list(by_group.items()) + list(inventory["commandsWithoutGroup"].items()):
        for row in positions:
            if not 0 <= row < len(commands) or commands[row]["ScenarioGroupID"] != group:
                raise ValueError(f"Incorrect command row {row} in {group}")
            partition[row] += 1
    if partition != Counter(range(len(commands))):
        raise ValueError("Each command row must occur in the index exactly once")
    branches = defaultdict(list)
    for branch in routes["branches"]:
        group = branch["group"]
        positions = by_group.get(group, ())
        if branch["row"] not in positions or any(x["row"] not in positions for x in branch["targets"]):
            raise ValueError(f"Branch outside its source group: {branch}")
        branches[group].append(branch)

    opening = {"tutorial_01", "main_01"}
    evidence = {item: "opening-story.md" for item in opening}
    evidence["pomodoroend_11"] = "work-conversation.md"
    result = {}
    for item, group in groups.items():
        category = scenario_types[int(group["Scenario"])]
        preliminary = {name: group[name] for name in ("UnlockLevel", "UnlockStoryEpisode", "ContentType", "PlayTiming")}
        preliminary["arguments"] = [group[key] for key in ("Arg1", "Arg2", "Arg3")]
        if category == "Tutorial":
            entry, persistence = "Startup tutorial and story log", "Tutorial completion and story log progress"
        elif category in {"MainScenario", "AfterScenario", "DLCScenario"}:
            entry, persistence = "ScenarioGroupMasterWrapper.PlayableScenario", "Check chapter progress and story log"
        else:
            entry, persistence = category, "Check any progress and temporary state for this trigger"
        result[item] = record(
            entry, preliminary, "Trigger the group and exercise each applicable choice",
            "Execute source commands in order with in-group branching",
            persistence, {"group": item, "category": category, "episode": group["EpisodeNumber"],
                          "commandRows": by_group[item], "branches": branches[item]},
            "Check chapter actions, body and face, camera, layout and window effects" if item in opening else
            "Confirm actual trigger, reachable branches and full room execution",
            [{"level": "player-partial", "record": evidence[item]}] if item in evidence else None)
    return result, sorted(row for positions in inventory["commandsWithoutGroup"].values() for row in positions)


def window_rows(scene, types):
    cases = scene["cases"]
    if set(cases) != {str(number) for number in types}:
        raise ValueError("Scene branches and window enum differ")
    result = {}
    for number, name in sorted(types.items()):
        branch = cases[str(number)]
        if branch["name"] != name:
            raise ValueError(f"Scene branch name disagrees with window enum: {number}")
        proof = {"Day": "windows-atmosphere.md", "Sunset": "windows-atmosphere.md",
                 "City": "city-window.md", "Cloudy": "cloudy-lighting.md"}.get(name)
        result[name] = record(
            "WindowViewService.ActivateWindow", {"scene": "RoomScene"}, name,
            "Enable the matching scene branch; verify scene output and branch interactions",
            "Check user selection in EnviromentData separately from isolated activation",
            {"windowViewType": number, "gameObject": branch["gameObject"]},
            "Check runtime switching and paired original frames",
            [{"level": "player-partial", "record": proof}] if proof else None)
    return result


def attach_window_references(windows, images):
    cases = images["isolated"]["cases"]
    if set(cases) != {str(row["source"]["windowViewType"]) for row in windows.values()}:
        raise ValueError("Isolated reference does not cover the scene branches")
    for name, row in windows.items():
        case = cases[str(row["source"]["windowViewType"])]
        if case["case"] != name:
            raise ValueError(f"Window reference image name disagrees with scene: {name}")
        row["evidence"].insert(0, {"level": "source-isolated", "record": "window-references.md",
                                   "sha256": case["sha256"]})


def unlock_rows(catalog, story, windows):
    result = {}
    for kind in ("environment", "decoration"):
        rows = {}
        for name, rule in catalog[kind].items():
            source = {"unlockItem": name, "windowCase": name if kind == "environment" and name in windows else None}
            policy = rule["policy"]
            if policy == "scenario" and rule["scenario"] not in story:
                raise ValueError(f"Unlock {name} has unknown story group: {rule['scenario']}")
            if policy == "purchase":
                entry, inputs, persistence = ("UnlockConditionService.Purchase", "Confirm purchase at source cost",
                                              "PointPurchaseData and point balance")
            elif policy in {"scenario", "always"}:
                entry, inputs = "UnlockConditionService.IsUnlocked", "Open selector and inspect availability"
                persistence = "Read PlayedScenarioGroupIDs" if policy == "scenario" else "None for eligibility"
            else:
                raise ValueError(f"Unknown unlock policy: {policy}")
            rows[name] = record(entry, rule, inputs, "Apply source rule and show matching item on selection",
                                persistence, source, "Check selection, appearance and reopening in the full room",
                                [{"level": "player-partial", "record": "ambient-access.md"}]
                                if kind == "environment" and name in {"HeavyRain", "LightRain", "City"} else None)
        result[kind] = rows
    return result


def settings(text):
    internal_fields = {"AmbientBGMVolumeBeforeStory", "AmbientSEVolumeBeforeStory", "IsAppliedTimerSeMigrationOnStartup"}
    fields = re.findall(r"^\s*public\s+([\w<>]+)\s+(\w+)\s*(?:=[^;\n]*)?;\s*$", text, re.MULTILINE)
    return {name: record(
        "SettingData", {"type": field_type}, "Change applicable Windows preference and reopen",
        "Apply original value range and restore the saved preference",
        "SettingData", {"field": name}, "Verify original settings UI, available choices and persistence",
        [{"level": "player-partial", "record": "room-language.md"}] if name == "GameLanguage" else None)
        for field_type, name in fields if name not in internal_fields}


def platform_rows():
    local = "Check source controls and local failure/recovery in the room"
    live = "Check real service with an authorized disposable profile"
    cases = {
        "windows-display": ("SettingService and ScreenSystem", "Change display and window states",
                            "SettingData", local),
        "local-media": ("FileBrowser and ImageContent", "Open valid and damaged local media; cancel picker",
                        "Media selection and preferences", local),
        "steam-start-dlc": ("SteamManager", "Start Steam and check installed content",
                            "Steam session and content eligibility", live),
        "steam-achievements": ("SteamAchievements", "Read, update and reopen user statistics",
                               "Steam user statistics", live),
        "game-account": ("LoginFlow", "Exercise account, consent, cancellation and maintenance",
                         "Authorized test account and profile", live),
        "game-account-sync": ("SaveDataSync", "Upload and download, then repeat after a connection failure",
                              "Publisher profile and local save stamps", live),
        "steam-cloud": ("Steam client", "Sync the same build and profile on two machines",
                        "Platform cloud and local saves", live),
        "news-media": ("LoginFlow and ImageContent", "Load, cancel and recover news or image requests",
                       "News state and image cache", local),
    }
    return {name: record(entry, {"profile": "isolated"}, inputs, "Match the associated platform acceptance case",
                         persistence, {"record": "platform-scenarios.md"}, gap,
                         [{"level": "player-partial", "record": "platform-scenarios.md#windows-窗口回归"}]
                         if name == "windows-display" else None)
            for name, (entry, inputs, persistence, gap) in cases.items()}


def file_hash(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def assemble(source, locks):
    names = ("story-inventory", "story-routes", "window-scenes", "window-references", "unlock-catalog")
    paths = {name: locks / (name + ".json") for name in names}
    identities = {name: file_hash(path) for name, path in paths.items()}
    inventory, routes, scene, images, unlocks = (json.loads(paths[name].read_bytes()) for name in names)
    if any(data["sourceSHA256"]["storyInventory"] != identities["story-inventory"] for data in (routes, unlocks)):
        raise ValueError("Catalogs have different story inventory identities")

    scripts = source / "Assets/Scripts/Assembly-CSharp/Bulbul"
    source_paths = ("MasterData/ScenarioType.cs", "ScenarioGroupMasterWrapper.cs", "WindowViewType.cs",
                    "WindowViewService.cs", "EnvironmentTypeExtensions.cs", "AmbientSoundType.cs",
                    "DecorationService.cs", "GameLanguageType.cs", "SettingData.cs", "SettingService.cs",
                    "AllPlatformSettingData.cs", "WindowModeType.cs", "WindowResolutionType.cs",
                    "UnlockConditionService.cs")
    path_for = {name: scripts / name for name in source_paths}
    path_for["DecorationService.cs"] = scripts.parent / "DecorationService.cs"
    source_hashes = {name: file_hash(path) for name, path in path_for.items()}
    if any(value != source_hashes["WindowViewType.cs"] for value in (scene["sourceEnumSHA256"], images["sourceEnumSHA256"])):
        raise ValueError("Scene and reference catalog have different window enum identities")
    read = lambda name: path_for[name].read_text(encoding="utf-8-sig")
    story, unassigned = story_rows(inventory, routes, enum_values(read("MasterData/ScenarioType.cs")))
    windows = window_rows(scene, enum_values(read("WindowViewType.cs")))
    attach_window_references(windows, images)
    items = unlock_rows(unlocks, story, windows)
    sounds = set(enum_values(read("AmbientSoundType.cs")).values())
    for name, row in items["environment"].items():
        sound = "Fireworks_First" if name == "Fireworks" else name
        row["source"]["ambientSound"] = sound if sound in sounds else None
        if not (row["source"]["ambientSound"] or row["source"]["windowCase"]):
            raise ValueError(f"Environment has no source audio or window branch: {name}")
    languages = {name: record("GameLanguageType", {"languageID": number}, "Switch language and reopen",
                              "Refresh visible source text while retaining current voice",
                              "SettingData.GameLanguage", {"languageID": number},
                              "Check remaining interfaces and operating system language selection",
                              [{"level": "player-partial", "record": "room-language.md"}])
                 for number, name in enum_values(read("GameLanguageType.cs")).items() if number != 0}
    preferences = settings(read("SettingData.cs"))
    for name, enum in (("WindowMode", "WindowModeType.cs"), ("WindowResolution", "WindowResolutionType.cs")):
        preferences[name]["source"]["values"] = enum_values(read(enum))
    if "FirstDayInWeek" not in read("AllPlatformSettingData.cs") or "ChangeFirstday" not in read("SettingService.cs"):
        raise ValueError("Missing first day of week settings source")
    preferences["FirstDayInWeek"] = record(
        "SettingService.ChangeFirstday", {"type": "DayOfWeek"}, "Set start of week and reopen the calendar",
        "Group and display the week starting on the selected day", "AllPlatformSettingData.FirstDayInWeek",
        {"field": "FirstDayInWeek"}, "Check first day of week in the statistics and calendar UI")
    rows = {"story": story, "window": windows, **items, "language": languages,
            "setting": preferences, "platform": platform_rows()}
    return {"schemaVersion": 1, "reference": "Recovery Windows 1.17.3", "catalogSHA256": identities,
            "sourceRoot": "Assets/Scripts/Assembly-CSharp", "sourceSHA256": {
                ("Bulbul/" if path.parent != scripts.parent else "") + name: source_hashes[name]
                for name, path in path_for.items()},
            "rows": rows, "unassignedStoryCommandRows": unassigned,
            "summary": {**{kind: len(values) for kind, values in rows.items()},
                        "storyCategories": len({row["source"]["category"] for row in story.values()}),
                        "storyCommands": len(inventory["commands"]), "branchTargets": routes["summary"]["targets"],
                        "environmentsWithWindow": sum(bool(row["source"]["windowCase"])
                                                      for row in items["environment"].values()),
                        "environmentsWithAudio": sum(bool(row["source"]["ambientSound"])
                                                     for row in items["environment"].values())}}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("source", "locks", "output"):
        parser.add_argument("--" + argument, type=Path, required=True)
    args = parser.parse_args()
    document = assemble(args.source, args.locks)
    with args.output.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
        stream.write("\n")
    print(json.dumps({"summary": document["summary"], "sha256": file_hash(args.output)}))
