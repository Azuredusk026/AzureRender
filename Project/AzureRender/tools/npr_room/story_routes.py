"""Resolve the Recovery story reader's explicit branches against source rows."""

import argparse
from collections import Counter, defaultdict
from datetime import datetime
from itertools import groupby
import json
from pathlib import Path
import re

from tools.npr_room.reference_inventory import digest


SCRIPTS = "Assets/Scripts/Assembly-CSharp/Bulbul"
COMMAND_ENUM = SCRIPTS + "/MasterData/CommandType.cs"
SCENARIO_READER = SCRIPTS + "/ScenarioReader.cs"
CHOICES = {"Selection", "TutorialSelection"}
ROW_TARGETS = CHOICES | {"Jump"}


def command_names(source):
    declarations = re.findall(r"(?m)^\s*(\w+)\s*=\s*(\d+)\s*,?\s*$", source)
    names = {int(number): name for name, number in declarations}
    if not names or len(names) != len(declarations) or len(set(names.values())) != len(names):
        raise ValueError("Invalid command enum declarations")
    return names


def branch_inventory(story, names):
    commands = story["commands"]
    values = [int(row["Command"]) for row in commands]
    use = Counter(values)
    missing = sorted(use.keys() - names.keys())
    if missing:
        raise ValueError(f"Unknown serialized command {missing[0]}")
    used = [{"value": value, "name": names[value], "rows": count} for value, count in sorted(use.items())]
    kind = [names[value] for value in values]
    branches, choice_blocks = [], []
    for group, rows in story["commandsByGroup"].items():
        matches = defaultdict(list)
        positions = {row: offset for offset, row in enumerate(rows)}
        for row in rows:
            matches[commands[row]["ID"]].append(row)

        def target(identity):
            candidates = matches.get(identity, [])
            return {"id": identity, "row": candidates[0] if candidates else None, "otherRows": candidates[1:]}

        for name, run in groupby(rows, key=kind.__getitem__):
            if name in CHOICES:
                choice_blocks.append({"group": group, "kind": name, "rows": list(run)})

        for row in rows:
            command = commands[row]
            name = kind[row]
            record = {"group": group, "row": row, "kind": name}
            if name in ROW_TARGETS:
                record["targets"] = [target(command["Arg1"])]
                if name == "TutorialSelection":
                    record["usedKey"] = command["Arg2"]
            elif name == "IfBetweenDateTime":
                window = command["Arg1"].split("~")
                if len(window) != 2 or any(len(value) != 14 for value in window):
                    raise ValueError(f"Invalid date interval at row {row}")
                begin, end = (datetime.strptime(value, "%Y%m%d%H%M%S") for value in window)
                if begin > end:
                    raise ValueError(f"Reversed date interval at row {row}")
                record["localInterval"] = window
                record["targets"] = [target(command[field]) for field in ("Arg3", "Arg4")]
            elif name == "StartTextLoop":
                anchor = target(command["Arg3"])
                record["targets"] = [anchor]
                options = []
                if anchor["row"] is not None:
                    for choice in rows[positions[anchor["row"]]:]:
                        if kind[choice] != "Selection":
                            break
                        options.append(choice)
                record["options"] = options
            else:
                continue
            branches.append(record)
    targets = [target for row in branches for target in row["targets"]]
    return {"usedCommands": used, "branches": branches, "choiceBlocks": choice_blocks,
            "summary": {"commandTypes": len(used), "branchRows": len(branches), "choiceBlocks": len(choice_blocks),
                        "targets": len(targets), "unresolvedTargets": sum(row["row"] is None for row in targets),
                        "duplicateTargets": sum(bool(row["otherRows"]) for row in targets)}}


def extract(source, inventory):
    enum, reader = source / COMMAND_ENUM, source / SCENARIO_READER
    story = json.loads(inventory.read_bytes())
    names = command_names(enum.read_text(encoding="utf-8-sig"))
    return {"formatVersion": 1, "sourceSHA256": {"commandEnum": digest(enum), "reader": digest(reader),
             "storyInventory": digest(inventory)}, **branch_inventory(story, names)}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("source", "inventory", "output"):
        parser.add_argument("--" + argument, type=Path, required=True)
    args = parser.parse_args()
    result = extract(args.source, args.inventory)
    with args.output.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(result, stream, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
        stream.write("\n")
    print(json.dumps(result["summary"]))
