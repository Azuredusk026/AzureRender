"""Stage Recovery room sound sources and the environment audio controls."""

import argparse
from html import escape
import json
from pathlib import Path
import re
import shutil
import tempfile
from uuid import UUID

from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest
from tools.npr_room.room_geometry import activate_room_control, copy_room_asset, stage_live_audio_media
from tools.npr_room.skinned_mesh_export import UnityScene


AMBIENT_MASTER = "Assets/App/BulbulAssets/MasterData/ScriptableObjects/AmbientMaster.asset"
AMBIENT_ENUM = "Assets/Scripts/Assembly-CSharp/Bulbul/AmbientSoundType.cs"
ACTIONS = ("toggle", "page-prev", "page-next", "quiet", "loud", "mute")


def ambient_catalog(source, audio_catalog):
    master, enum = source / AMBIENT_MASTER, source / AMBIENT_ENUM
    names = {int(code): name for name, code in re.findall(r"(?m)^\s*(\w+)\s*=\s*(\d+)\s*,?\s*$",
                                                         enum.read_text(encoding="utf-8-sig"))}
    slots = json.loads(audio_catalog.read_bytes())["slots"]
    sounds = []
    seen = set()
    for row in UnityScene(master).read(11400000)["AmbientSounds"]:
        code, clip = row["AmbientSound"], row["AudioClip"]
        name = names[code]
        guid = UUID(clip["guid"])
        if code in seen or clip["fileID"] != 8300000 or clip["type"] != 3:
            raise ValueError(f"Invalid ambient sound identity: {code}")
        seen.add(code)
        entry = slots[guid.hex]
        if Path(entry["source"]).suffix.lower() != ".ogg":
            raise ValueError(f"Expected original ambient sound container: {code}")
        title = re.sub(r"(?<=[a-z])(?=[A-Z])", " ", name.replace("_", " "))
        sounds.append({"id": code, "uuid": str(guid), "guid": guid.hex, "fileID": clip["fileID"], "name": title,
                       "loop": name not in ("Fireworks_First", "Fireworks_Second"), "source": entry["source"],
                       "sha256": entry["sha256"], "asset": f"ambient/{guid}.ogg"})
    if seen != set(names) or not any(sound["loop"] for sound in sounds):
        raise ValueError("Ambient master does not cover the original sound IDs")
    return {"schemaVersion": 1, "sourceSHA256": digest(master), "enumSHA256": digest(enum), "sounds": sounds}


def ambient_config(receipt):
    rows = []
    for row in receipt["sounds"]:
        if row["loop"]:
            values = (f"room-ambient-{row['id']}", "assets:/" + row["asset"], row["name"])
            fields = ", ".join(json.dumps(value, ensure_ascii=False) for value in values)
            rows.append(f"    {{{row['id']}, {fields}}},")
    return "local sounds = {\n" + "\n".join(rows) + "\n}\n"


def stage_ambient(room, output, source, audio_catalog):
    example = Path(__file__).resolve().parents[2] / "examples/npr-room/assets"
    receipt = ambient_catalog(source, audio_catalog)
    loops = [row for row in receipt["sounds"] if row["loop"]]
    descriptor = json.loads((room / "project.azureproject").read_bytes())
    with tempfile.TemporaryDirectory(prefix="room-ambient-", dir=output.parent) as parent:
        project = Path(parent) / "project"
        assets = project / "assets"
        shutil.copytree(room / "assets", assets, copy_function=copy_room_asset, ignore=shutil.ignore_patterns(".azure"))
        level_path, ui_path = assets / "room.azurelevel", assets / "timer.rml"
        level = json.loads(level_path.read_bytes())
        ui = ui_path.read_text(encoding="utf-8")
        actions = [f"select-{row['id']}" for row in loops]
        activate_room_control(descriptor, level, "ambient", (*ACTIONS, *actions))
        for row in loops:
            level["nodes"].append({"id": f"room-ambient-{row['id']}", "components": {
               "azure.audio-source": {"type": "azure.audio-source", "version": 1, "data": {"loop": True}},
            }})
        listing = "\n".join(
            f'<button id="ambient-{action}" data-action="ambient-{action}">{escape(row["name"])}</button>'
            for action, row in zip(actions, loops, strict=True)
        )
        controls = (example / "ambient.rml").read_text(encoding="utf-8").replace("{{sounds}}", listing)
        style, body = controls.split("</style>\n", 1)
        ui = ui.replace("</style>", style + "</style>", 1).replace("</body>", body + "</body>", 1)
        stage_live_audio_media(source, receipt["sounds"], assets)
        write(project / "project.azureproject", descriptor)
        write(project / "ambient-receipt.json", receipt)
        shutil.copyfile(room / "music-receipt.json", project / "music-receipt.json")
        write(level_path, level)
        ui_path.write_text(ui, encoding="utf-8", newline="\n")
        script = ambient_config(receipt) + (example / "ambient.lua").read_text(encoding="utf-8")
        (assets / "ambient.lua").write_text(script, encoding="utf-8", newline="\n")
        publish_directory(project, output)
    return {"sounds": len(receipt["sounds"]), "loops": len(loops), "sourceSHA256": receipt["sourceSHA256"]}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("room", "source", "output", "audio-catalog"):
        parser.add_argument("--" + argument, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(stage_ambient(args.room, args.output, args.source, args.audio_catalog)))
