"""Preserve Recovery story rows and index the static relationships between them."""

import argparse
from collections import Counter, defaultdict
from hashlib import file_digest
import json
from pathlib import Path

import yaml


TABLES = {
    "groups": "ScenarioGroupMaster",
    "commands": "NovelMaster",
    "localization": "LocalizationMaster",
}


def index_story(groups, commands, localization):
    commands_by_group = {row["ID"]: [] for row in groups}
    translations = {row["ID"] for row in localization}
    if len(commands_by_group) != len(groups) or len(translations) != len(localization):
        raise ValueError("Group IDs and localization IDs must each be unique")
    kinds = Counter()
    command_ids = defaultdict(list)
    commands_without_group = defaultdict(list)
    unresolved_localization = defaultdict(list)
    for index, row in enumerate(commands):
        group = row["ScenarioGroupID"]
        if group in commands_by_group:
            commands_by_group[group].append(index)
        else:
            commands_without_group[group].append(index)
        localization_id = row["LocalizationID"]
        if localization_id not in ("", "0") and localization_id not in translations:
            unresolved_localization[localization_id].append(index)
        kinds[row["Command"]] += 1
        command_ids[row["ID"]].append(index)
    return {
        "commandsByGroup": commands_by_group,
        "commandsWithoutGroup": dict(commands_without_group),
        "unresolvedLocalization": dict(unresolved_localization),
        "duplicateCommandIDs": {key: positions for key, positions in command_ids.items() if len(positions) > 1},
        "commandKinds": dict(kinds),
    }


def extract(project):
    master = project / "Assets/App/BulbulAssets/MasterData/ScriptableObjects/Generated"
    tables, identities = {}, {}
    loader = getattr(yaml, "CBaseLoader", yaml.BaseLoader)
    for slot, name in TABLES.items():
        path = master / (name + ".asset")
        with path.open("rb") as stream:
            identities[slot] = file_digest(stream, "sha256").hexdigest()
        lines = path.read_text(encoding="utf-8-sig").split("\n", 3)
        if len(lines) != 4 or not lines[2].startswith("--- !u!114 "):
            raise ValueError(f"Expected a Unity MonoBehaviour asset: {path.name}")
        tables[slot] = yaml.load(lines[3], Loader=loader)["MonoBehaviour"]["Items"]
    return {
        "formatVersion": 1,
        "sourceSHA256": identities,
        **tables,
        **index_story(tables["groups"], tables["commands"], tables["localization"]),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    data = extract(args.source)
    with args.output.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(data, stream, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
        stream.write("\n")
    print(json.dumps({
        "groups": len(data["groups"]),
        "commandRows": len(data["commands"]),
        "commandKinds": len(data["commandKinds"]),
        "localizationRows": len(data["localization"]),
        "duplicateCommandIDs": len(data["duplicateCommandIDs"]),
        "emptyGroups": sum(not rows for rows in data["commandsByGroup"].values()),
        "commandsWithoutGroup": sum(map(len, data["commandsWithoutGroup"].values())),
        "unresolvedLocalizedCommands": sum(map(len, data["unresolvedLocalization"].values())),
    }))


if __name__ == "__main__":
    main()
