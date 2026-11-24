"""Resolve the explicit Resources and Addressables calls in the Recovery application."""

import argparse
import json
from pathlib import Path
import re

from tools.npr_room.reference_inventory import digest, text


CALL = re.compile(
    r'\b(?P<api>Resources\.Load(?:Async|All)?|Addressables\.(?:InitializeAsync|LoadAssets?Async|InstantiateAsync))'
    r'(?:<(?P<type>\w+)>)?\((?P<key>"[^"\n]+"(?:\s*\+\s*\w+\.ToName\(\))?|\w+)?\s*[,)]'
)
RESOURCE_KINDS = "Assets/Scripts/Assembly-CSharp/Bulbul/WebEnvKind.cs"
NEWS = "Assets/App/BulbulAssets/MasterData/ScriptableObjects/NewsMaster.asset"


def source_calls(serialized):
    return [{"api": match["api"], "type": match["type"], "keyExpression": match["key"] or "",
             "line": serialized.count("\n", 0, match.start()) + 1}
            for match in CALL.finditer(serialized)]


def addressable_targets(catalog, key, asset_type):
    indices = [index for row in catalog["keys"] if row["value"] == key for index in row["locations"]]
    if asset_type and asset_type != "ScriptableObject":
        indices = [index for index in indices if
                   catalog["locations"][index]["resourceType"]["m_ClassName"].rsplit(".", 1)[-1] == asset_type]
    return indices


def resource_targets(resources, key):
    return [path for indexed, paths in resources.items() if key.casefold() == indexed.casefold() for path in paths]


def image_keys(serialized):
    return sorted({key for key in re.findall(r"!\[[^\]]*\]\(([^\s)]+)\)", serialized)
                   if ":" not in key and not key.startswith("//")})


def build_report(source, reference_inventory, recovery):
    inventory = json.loads(reference_inventory.read_bytes())
    catalog = inventory["catalog"]
    web_profiles = re.findall(r"(?m)^\s*(\w+)\s*=\s*\d+\s*,?\s*$", text(source / RESOURCE_KINDS))
    local_image_keys = image_keys(text(source / NEWS))
    code = source / "Assets/Scripts/Assembly-CSharp"
    calls = []
    for path in sorted(code.rglob("*.cs")):
        rows = source_calls(text(path))
        if not rows:
            continue
        for row in rows:
            row["source"] = path.relative_to(source).as_posix()
            row["sha256"] = digest(path)
            key = row["keyExpression"]
            if row["api"] == "Addressables.InitializeAsync":
                row["catalogSHA256"] = catalog["sha256"]
            elif row["api"].startswith("Addressables."):
                keys = [json.loads(key)] if key.startswith('"') else local_image_keys if key == "url" else []
                row["targets"] = {name: addressable_targets(catalog, name, row["type"]) for name in keys}
            elif row["api"].startswith("Resources."):
                if key == '"WebEnv/" + kind.ToName()':
                    keys = ["WebEnv/" + profile for profile in web_profiles]
                else:
                    keys = [json.loads(key)] if key.startswith('"') and "+" not in key else []
                row["targets"] = {name: resource_targets(inventory["resources"], name) for name in keys}
            calls.append(row)

    evidence = recovery / "Evidence/standalone-compiled-final-verification.json"
    verification = json.loads(text(evidence))
    if not verification["passed"] or "Boot reached RoomScene" not in verification["observations"]:
        raise ValueError("Missing Recovery standalone startup evidence")
    streams = recovery / "Build/ChillWithYouRecovery_Data/StreamingAssets"
    return {
        "formatVersion": 1, "inventorySHA256": digest(reference_inventory), "calls": calls,
        "standaloneStartup": {"evidenceSHA256": digest(evidence), "startupReachedRoom": True},
        "streamingAssets": {item.relative_to(streams).as_posix(): {"sha256": digest(item), "bytes": item.stat().st_size}
                            for item in sorted(streams.rglob("*")) if item.is_file()},
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("source", "inventory", "recovery", "output"):
        parser.add_argument("--" + argument, type=Path, required=True)
    args = parser.parse_args()
    report = build_report(args.source, args.inventory, args.recovery)
    with args.output.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(report, stream, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
        stream.write("\n")
    print(json.dumps({"calls": len(report["calls"]), "streamingAssets": len(report["streamingAssets"])}))


if __name__ == "__main__":
    main()
