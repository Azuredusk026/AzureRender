"""Match original MasterData loader branches to assets and candidate source receipts."""

import argparse
from collections import defaultdict
import json
from pathlib import Path
import re

from tools.npr_room.dynamic_load_inventory import addressable_targets, source_calls
from tools.npr_room.reference_inventory import digest, text


SOURCE_FOLDER = "Assets/Scripts/Assembly-CSharp/Bulbul"
RECEIPTS = {"GameParameterMaster": "work-progression-receipt.json",
            "MusicMaster": "music-receipt.json", "AmbientMaster": "ambient-receipt.json"}
STORY_TABLES = {"ScenarioGroupMaster": "groups", "NovelMaster": "commands", "LocalizationMaster": "localization"}
UNLOCK_TABLES = {"UnlockEnvironmentMaster": "environment", "UnlockDecorationMaster": "decoration"}


def loader_exports(loader):
    branches = dict(re.findall(r"\bitem is (\w+)\s+(\w+)\b", loader))
    result = {}
    for resource, variable in branches.items():
        fields = re.findall(r"\b(\w+)\s*=\s*" + re.escape(variable) + r"\s*(?=[.;])", loader)
        if not fields or len(fields) != len(set(fields)):
            raise ValueError(f"Expected distinct loader fields for {resource}")
        result[resource] = fields
    return result


def member_reads(source, fields):
    receivers = set(re.findall(r"\bMasterDataLoader\s+(\w+)\b", source))
    if not receivers:
        return {}
    qualifier = "|".join(map(re.escape, sorted(receivers)))
    occurrence = re.compile(r"\b(?:" + qualifier + r")\s*\.\s*(\w+)\b")
    reads = defaultdict(list)
    for number, line in enumerate(source.splitlines(), start=1):
        for field in sorted(set(occurrence.findall(line)) & fields):
            reads[field].append(number)
    return dict(reads)


def consumers(source, exports):
    fields = {field for members in exports.values() for field in members}
    report = defaultdict(list)
    for path in sorted((source / "Assets/Scripts/Assembly-CSharp").rglob("*.cs")):
        if path.name == "MasterDataLoader.cs":
            continue
        matches = member_reads(text(path), fields)
        if not matches:
            continue
        relative = path.relative_to(source).as_posix()
        source_hash = digest(path)
        for field, lines in matches.items():
            report[field].append({"path": relative, "sha256": source_hash, "lines": lines})
    return report


def assembled_sources(project, locks):
    references = {}
    for name, tables, confirmation in (
        ("story-inventory.json", STORY_TABLES, ("opening-story-room-receipt.json", "inventorySHA256")),
        ("unlock-catalog.json", UNLOCK_TABLES, ("unlock-receipt.json", "unlockCatalogSHA256")),
    ):
        path = locks / name
        artifact = json.loads(path.read_bytes())
        fingerprint = digest(path)
        receipt = project.parent / confirmation[0]
        if json.loads(receipt.read_bytes())[confirmation[1]] != fingerprint:
            raise ValueError(f"Room assembly does not match pinned {name}")
        for resource, key in tables.items():
            references[resource] = {"file": name, "sha256": fingerprint, "sourceSHA256": artifact["sourceSHA256"][key],
                                    "roomReceipt": receipt.name, "roomReceiptSHA256": digest(receipt)}
    return references


def master_locations(catalog, loader):
    branches = re.findall(r"\bitem is (\w+)\s+\w+\b", loader)
    rows = []
    types = set()
    for index in addressable_targets(catalog, "MasterData", "ScriptableObject"):
        item = catalog["locations"][index]
        resource_type = item["resourceType"]["m_ClassName"]
        short_type = resource_type.rsplit(".", 1)[-1]
        if short_type in types:
            raise ValueError(f"Duplicate MasterData location type: {short_type}")
        types.add(short_type)
        rows.append({"location": index, "asset": item["internalId"], "type": resource_type})
    if not branches or set(branches) != types or len(branches) != len(types):
        raise ValueError("The MasterData label differs from the loader's typed branches")
    return rows


def build_report(source, inventory, project, locks=None):
    reference = json.loads(inventory.read_bytes())
    loader = source / SOURCE_FOLDER / "MasterDataLoader.cs"
    entry = source / SOURCE_FOLDER / "EntryBehavior.cs"
    loader_text = text(loader)
    calls = [call for call in source_calls(loader_text) if
             (call["api"], call["type"], call["keyExpression"]) ==
             ("Addressables.LoadAssetsAsync", "ScriptableObject", '"MasterData"')]
    if len(calls) != 1 or "await UniTask.WaitUntil(() => masterDataLoader.IsLoaded)" not in text(entry):
        raise ValueError("The original MasterData load and entry wait must both be present")

    rows = master_locations(reference["catalog"], loader_text)
    root = source.resolve()
    for row in rows:
        relative = Path(row["asset"])
        target = (root / relative).resolve()
        if (not relative.parts or relative.parts[0] != "Assets" or not target.is_relative_to(root / "Assets")
                or row["asset"] not in reference["assets"]):
            raise ValueError(f"MasterData target is outside the pinned source inventory: {row['asset']}")
        row["sourceSHA256"] = digest(target)
        row["sourceGuid"] = reference["assets"][row["asset"]]
        short_type = row["type"].rsplit(".", 1)[-1]
        if short_type in RECEIPTS:
            receipt = project.parent / RECEIPTS[short_type]
            if json.loads(receipt.read_bytes())["sourceSHA256"] != row["sourceSHA256"]:
                raise ValueError(f"Candidate source digest differs: {receipt.name}")
            row["candidateSourceReceipt"] = {"file": receipt.name, "sha256": digest(receipt)}
    report = {"schemaVersion": 1, "sourceInventorySHA256": digest(inventory), "loaderSHA256": digest(loader),
              "entrySHA256": digest(entry), "projectSHA256": digest(project), "initialization": calls[0], "assets": rows}
    if locks is not None:
        exports = loader_exports(loader_text)
        paths = consumers(source, exports)
        assembly = assembled_sources(project, locks)
        report["schemaVersion"] = 2
        for row in rows:
            resource = row["type"].rsplit(".", 1)[-1]
            row["loaderFields"] = {field: paths.get(field, []) for field in exports[resource]}
            if resource in assembly:
                artifact = assembly[resource]
                if artifact["sourceSHA256"] != row["sourceSHA256"]:
                    raise ValueError(f"Generated input has a different MasterData source: {resource}")
                row["candidateGeneratedInput"] = artifact
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for field in ("source", "inventory", "project", "output"):
        parser.add_argument("--" + field, type=Path, required=True)
    parser.add_argument("--locks", type=Path)
    args = parser.parse_args()
    report = build_report(args.source, args.inventory, args.project, args.locks)
    with args.output.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(report, stream, indent=2, sort_keys=True)
        stream.write("\n")


if __name__ == "__main__":
    main()
