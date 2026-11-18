"""Match original MasterData loader branches to assets and candidate source receipts."""

import argparse
import json
from pathlib import Path
import re

from tools.npr_room.dynamic_load_inventory import addressable_targets, source_calls
from tools.npr_room.reference_inventory import digest, text


SOURCE_FOLDER = "Assets/Scripts/Assembly-CSharp/Bulbul"
RECEIPTS = {"GameParameterMaster": "work-progression-receipt.json",
            "MusicMaster": "music-receipt.json", "AmbientMaster": "ambient-receipt.json"}


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


def build_report(source, inventory, project):
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
    return {"schemaVersion": 1, "sourceInventorySHA256": digest(inventory), "loaderSHA256": digest(loader),
            "entrySHA256": digest(entry), "projectSHA256": digest(project), "initialization": calls[0], "assets": rows}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for field in ("source", "inventory", "project", "output"):
        parser.add_argument("--" + field, type=Path, required=True)
    args = parser.parse_args()
    report = build_report(args.source, args.inventory, args.project)
    with args.output.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(report, stream, indent=2, sort_keys=True)
        stream.write("\n")


if __name__ == "__main__":
    main()
