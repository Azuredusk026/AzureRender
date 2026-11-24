"""Verify room sound mounts against Recovery timer and system sound tables."""

import argparse
from hashlib import sha256
import json
from pathlib import Path
from uuid import UUID

from tools.npr_room.audio_repair import WAVE_HEADER, repair_wave_header
from tools.npr_room.feature_matrix import enum_values
from tools.npr_room.master_data_inventory import SOURCE_FOLDER
from tools.npr_room.reference_inventory import digest, group_paths_by_guid, text
from tools.npr_room.skinned_mesh_export import UnityScene


TABLE_ROOT = "Assets/App/BulbulAssets/MasterData/ScriptableObjects"
BINDINGS = {
    "Timer": {"Start_A": "timer-start", "Finish_A": "timer-finish", "Pause": "timer-pause"},
    "System": {"Cancel": "settings-cancel", "Click": "settings-click", "TaskComplete": "todo-complete"},
}
TIMER_VARIANTS = {"Start_B": "timer-start-b", "Start_C": "timer-start-c",
                  "Finish_B": "timer-finish-b", "Finish_C": "timer-finish-c"}


def verify_media(source, target, repairs):
    source_sha = digest(source)
    target_sha = digest(target)
    report = {"sourceSHA256": source_sha, "targetSHA256": target_sha}
    if source_sha == target_sha and source.suffix.lower() == target.suffix.lower() == ".ogg":
        return report
    if source.suffix.lower() == target.suffix.lower() == ".wav":
        original = source.read_bytes()
        fixed = repair_wave_header(original)
        pcm = sha256(original[WAVE_HEADER.size:]).hexdigest()
        receipt = repairs.get(f"media/{source_sha}.wav", {})
        if (fixed is not None and sha256(fixed).hexdigest() == target_sha
                and (receipt.get("sourceSHA256"), receipt.get("sha256"), receipt.get("pcmSHA256"))
                == (source_sha, target_sha, pcm)):
            return dict(report, pcmSHA256=pcm)
    raise ValueError(f"Sound media identity differs for {target.name}")


def read_table(source, assets, kind):
    master_path = f"{TABLE_ROOT}/{kind}SeMaster.asset"
    master = source / master_path
    enum = source / SOURCE_FOLDER / f"{kind}SeType.cs"
    names = enum_values(text(enum))
    clips = UnityScene(master).read(11400000)[kind + "Ses"]
    codes = [clip["SeSound"] for clip in clips]
    if len(codes) != len(names) or set(codes) != names.keys():
        raise ValueError(f"Sound rows differ from enum {enum.name}")
    return names, clips, {"master": master_path, "masterSHA256": digest(master),
                          "sourceGuid": assets[master_path], "enumSHA256": digest(enum)}


def build_report(source, inventory, room, audio_repairs, purchase=False, timer_variants=False):
    assets = json.loads(inventory.read_bytes())["assets"]
    by_guid = group_paths_by_guid(assets)
    repairs = json.loads(audio_repairs.read_bytes())["repairs"]
    root = room.resolve(strict=True)
    project = root / "project.azureproject"
    project_spec = json.loads(project.read_bytes())
    mount, = (item for item in project_spec["mounts"] if item["name"] == "assets")
    target_root = (root / mount["path"]).resolve(strict=True)
    if not target_root.is_relative_to(root):
        raise ValueError("The room assets mount escapes the room")
    level = target_root / "room.azurelevel"
    nodes = json.loads(level.read_bytes())["nodes"]
    source_root = (source / "Assets").resolve(strict=True)
    report = {"schemaVersion": 1, "inventorySHA256": digest(inventory), "repairsSHA256": digest(audio_repairs),
              "projectSHA256": digest(project), "levelSHA256": digest(level), "tables": {}}

    for kind, bindings in BINDINGS.items():
        if timer_variants and kind == "Timer":
            bindings = bindings | TIMER_VARIANTS
        if purchase and kind == "System":
            bindings = dict(bindings, BuyItem="room-buy-item")
        names, clips, table = read_table(source, assets, kind)
        table["clips"] = []
        for clip in clips:
            code = clip["SeSound"]
            name = names[code]
            audio = clip["AudioClip"]
            relative, = by_guid[audio["guid"]]
            raw = (source / relative).resolve(strict=True)
            if not raw.is_relative_to(source_root) or audio["fileID"] != 8300000:
                raise ValueError(f"Invalid Unity AudioClip for {kind}.{name}")
            row = {"cue": name, "code": code, "source": relative, "guid": audio["guid"],
                   "fileID": audio["fileID"], "sourceSHA256": digest(raw)}
            if name in bindings:
                slot = bindings[name]
                node, = (item for item in nodes if item["id"] == slot)
                data = node["components"]["azure.audio-source"]["data"]
                reference = data["asset"]
                if not reference.startswith("assets:/"):
                    raise ValueError(f"Sound cue has an invalid asset reference: {slot}")
                target = (target_root / reference.removeprefix("assets:/")).resolve(strict=True)
                if not target.is_relative_to(target_root):
                    raise ValueError(f"Sound cue escapes the assets mount: {slot}")
                row["roomBinding"] = {"node": slot, "asset": reference, "volume": data["volume"],
                                      **verify_media(raw, target, repairs)}
                if slot == "room-buy-item" or slot in TIMER_VARIANTS.values():
                    meta = target.with_name(target.name + ".azmeta")
                    if UUID(json.loads(meta.read_bytes())["id"]).hex != audio["guid"]:
                        raise ValueError(f"Sound metadata differs from the original GUID: {slot}")
                    row["roomBinding"]["metadataSHA256"] = digest(meta)
            table["clips"].append(row)
        report["tables"][kind] = table
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source", "inventory", "room", "audio-repairs", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--purchase", action="store_true")
    parser.add_argument("--timer-variants", action="store_true")
    args = parser.parse_args()
    report = build_report(args.source, args.inventory, args.room, args.audio_repairs, args.purchase, args.timer_variants)
    with args.output.open("x", encoding="utf-8", newline="\n") as output:
        json.dump(report, output, indent=2, sort_keys=True)
        output.write("\n")


if __name__ == "__main__":
    main()
