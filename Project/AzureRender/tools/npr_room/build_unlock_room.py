"""Apply the source environment access rules to the integrated Windows room."""

import argparse
import json
from pathlib import Path
import shutil
import tempfile

from tools.npr_room.build_ambient_room import AMBIENT_ENUM, ambient_config, ambient_names
from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest
from tools.npr_room.room_geometry import copy_room_asset


def access_config(ambient, catalog, names):
    lines = ["local environment_access = {"]
    for row in ambient["sounds"]:
        if not row["loop"]:
            continue
        name = names[row["id"]]
        rule = catalog["environment"][name]
        fields = (rule["policy"], rule.get("scenario", rule.get("cost", 0)), rule.get("expires", ""), name)
        values = ", ".join(json.dumps(value) for value in fields)
        lines.append(f"    [{row['id']}] = {{{values}}},")
    lines.append("}")
    return "\n".join(lines) + "\n"


def stage_unlocks(room, output, source, unlock_catalog):
    examples = Path(__file__).resolve().parents[2] / "examples/npr-room/assets"
    ambient = json.loads((room / "ambient-receipt.json").read_bytes())
    enum = source / AMBIENT_ENUM
    if digest(enum) != ambient["enumSHA256"]:
        raise ValueError("The room's environment sounds use a different enum source")
    catalog = json.loads(unlock_catalog.read_bytes())
    program = access_config(ambient, catalog, ambient_names(enum)).encode("utf-8")
    binding = (examples / "ambient-access.lua").read_bytes()
    script = (examples / "ambient.lua").read_bytes()
    with tempfile.TemporaryDirectory(prefix="room-unlocks-", dir=output.parent) as parent:
        project = Path(parent) / "project"
        assets = project / "assets"
        shutil.copytree(room / "assets", assets, copy_function=copy_room_asset, ignore=shutil.ignore_patterns(".azure"))
        for name in ("project.azureproject", "choice-receipt.json", "work-progression-receipt.json",
                     "voice-receipt.json", "music-receipt.json", "ambient-receipt.json"):
            shutil.copyfile(room / name, project / name)
        if (room / "unlock-receipt.json").exists():
            raise ValueError("The room already uses environment access rules")
        (assets / "ambient.lua").write_bytes(b"\n".join((program, binding, ambient_config(ambient).encode("utf-8"), script)))
        receipt = {"schemaVersion": 1, "unlockCatalogSHA256": digest(unlock_catalog), "appliedLoops": sum(
            row["loop"] for row in ambient["sounds"])}
        write(project / "unlock-receipt.json", receipt)
        publish_directory(project, output)
    return receipt


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("room", "output", "source", "unlock-catalog"):
        parser.add_argument("--" + argument, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(stage_unlocks(args.room, args.output, args.source, args.unlock_catalog)))
