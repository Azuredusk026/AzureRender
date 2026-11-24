"""Index the original character controller using serialized Unity identities."""

import argparse
from collections import Counter
import json
from pathlib import Path

from tools.npr_room.reference_inventory import asset_index, digest, group_paths_by_guid
from tools.npr_room.skinned_mesh_export import UnityScene


CONTROLLER = "Assets/AnimatorController/Character_Animator_ForScript.controller"
GROUPS = {1107: "machines", 1102: "states", 1101: "stateTransitions",
          1109: "entryAndMachineTransitions", 114: "behaviours"}
OWNERSHIP_FIELDS = {"m_ObjectHideFlags", "m_CorrespondingSourceObject", "m_PrefabInstance", "m_PrefabAsset"}


def serialized_references(value, field):
    if isinstance(value, dict):
        if "fileID" in value:
            if value["fileID"]:
                yield field, value
            return
        for key, child in value.items():
            yield from serialized_references(child, f"{field}/{key}" if field else key)
    elif isinstance(value, list):
        for index, child in enumerate(value):
            yield from serialized_references(child, f"{field}/{index}")


def index_controller(records, paths_by_guid):
    (controller_id, controller), = [(file_id, data) for file_id, (kind, data) in records.items() if kind == 91]
    grouped = {group: {} for group in GROUPS.values()}
    external = {}
    local_refs = Counter()
    unresolved_local = []
    for file_id, (kind, data) in records.items():
        fields = {key: value for key, value in data.items() if key not in OWNERSHIP_FIELDS}
        if file_id != controller_id:
            grouped[GROUPS[kind]][str(file_id)] = fields
        for field, reference in serialized_references(fields, ""):
            target = reference["fileID"]
            guid = reference.get("guid")
            if guid:
                external[guid] = sorted(paths_by_guid.get(guid, ()))
            elif target not in records:
                unresolved_local.append({"owner": file_id, "field": field, "fileID": target})
            elif target != file_id:
                local_refs[target] += 1

    orphan_records = sorted(set(records) - local_refs.keys() - {controller_id})
    return {
        "name": controller["m_Name"], "fileID": controller_id,
        "parameters": controller["m_AnimatorParameters"], "layers": controller["m_AnimatorLayers"],
        **grouped, "externalAssets": external, "unresolvedLocal": unresolved_local, "unattachedRecords": orphan_records,
        "summary": {
            "layers": len(controller["m_AnimatorLayers"]), "parameters": len(controller["m_AnimatorParameters"]),
            **{group: len(items) for group, items in grouped.items()},
            "externalAssets": len(external), "unresolvedExternal": sum(not paths for paths in external.values()),
            "unresolvedLocal": len(unresolved_local), "unattachedRecords": len(orphan_records),
        },
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    path = args.source / CONTROLLER
    by_guid = group_paths_by_guid(asset_index(args.source))
    controller = UnityScene(path)
    records = {file_id: (kind, controller.read(file_id)) for file_id, (kind, _, _) in controller.documents.items()}
    manifest = index_controller(records, by_guid)
    manifest.update(formatVersion=1, source=CONTROLLER, sourceSHA256=digest(path))
    with args.output.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(manifest, stream, ensure_ascii=False, separators=(",", ":"), sort_keys=True)
        stream.write("\n")
    print(json.dumps(manifest["summary"]))


if __name__ == "__main__":
    main()
