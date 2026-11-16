"""Extract original environment and decoration unlock conditions and prices."""

import argparse
from collections import Counter
from datetime import datetime
import json
from pathlib import Path

from tools.npr_room.reference_inventory import digest
from tools.npr_room.skinned_mesh_export import UnityScene


MASTER = "Assets/App/BulbulAssets/MasterData/ScriptableObjects/Generated"
TABLES = {"environment": "UnlockEnvironmentMaster", "decoration": "UnlockDecorationMaster"}


def item_rules(rows, scenario_ids):
    result = {}
    for row in rows:
        item, condition, argument = row["ItemType"], row["ConditionsType"], row["Arg1"]
        if not item or item in result:
            raise ValueError(f"Duplicate or empty unlock item: {item}")
        rule = {"sourceCondition": condition, "arguments": [argument, row["Arg2"], row["Arg3"]],
                "mobileDemoLocked": bool(row["IsMobileDemoUserLockedTarget"])}
        if condition == "None" or (condition == "Scenario" and argument == "None"):
            rule["policy"] = "always"
        elif condition == "Scenario":
            if argument not in scenario_ids:
                raise ValueError(f"Unknown scenario for {item}: {argument}")
            rule.update(policy="scenario", scenario=argument)
        elif condition == "PointPurchase":
            price = int(argument)
            if price < 0:
                raise ValueError(f"Invalid price for {item}: {price}")
            rule.update(policy="purchase", cost=price)
        else:
            raise ValueError(f"Unknown unlock condition for {item}: {condition}")
        expiry = str(row.get("UnlockExpire", 0))
        if expiry != "0":
            if len(expiry) != 14:
                raise ValueError(f"Invalid unlock date for {item}: {expiry}")
            datetime.strptime(expiry, "%Y%m%d%H%M%S")
            rule["expires"] = expiry
        result[item] = rule
    return result


def extract(source, inventory):
    story = json.loads(inventory.read_bytes())
    ids = {row["ID"] for row in story["groups"]}
    result = {"schemaVersion": 1, "sourceSHA256": {"storyInventory": digest(inventory)}, "summary": {}}
    for group, table in TABLES.items():
        path = source / MASTER / (table + ".asset")
        rules = item_rules(UnityScene(path).read(11400000)["Items"], ids)
        result[group] = rules
        result["sourceSHA256"][group] = digest(path)
        result["summary"][group] = {"items": len(rules), **Counter(row["policy"] for row in rules.values()),
                                    "dated": sum("expires" in row for row in rules.values())}
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("source", "inventory", "output"):
        parser.add_argument("--" + argument, type=Path, required=True)
    args = parser.parse_args()
    document = extract(args.source, args.inventory)
    with args.output.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
        stream.write("\n")
    print(json.dumps(document["summary"]))
