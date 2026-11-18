"""Stage the original timer cue variants in an existing NPR room."""

import argparse
import json
from pathlib import Path
import shutil
import tempfile
from uuid import UUID

from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest, group_paths_by_guid
from tools.npr_room.room_geometry import copy_room_asset, stage_live_audio_media
from tools.npr_room.sound_cue_inventory import BINDINGS, TIMER_VARIANTS, read_table


def timer_plan(source, inventory):
    known_assets = json.loads(inventory.read_bytes())["assets"]
    by_guid = group_paths_by_guid(known_assets)
    names, clips, table = read_table(source, known_assets, "Timer")
    if set(names.values()) != BINDINGS["Timer"].keys() | TIMER_VARIANTS.keys():
        raise ValueError("Timer source does not match the room cue names")
    source_root = (source / "Assets").resolve(strict=True)
    sounds = []
    for clip in clips:
        code = clip["SeSound"]
        name = names[code]
        if name not in TIMER_VARIANTS:
            continue
        audio = clip["AudioClip"]
        guid = audio["guid"]
        relative, = by_guid[guid]
        raw = (source / relative).resolve(strict=True)
        if not raw.is_relative_to(source_root) or raw.suffix.lower() != ".ogg" or audio["fileID"] != 8300000:
            raise ValueError(f"Invalid original timer cue: {name}")
        slot = TIMER_VARIANTS[name]
        sounds.append({"cue": name, "code": code, "node": slot, "source": relative,
                       "asset": f"timer-cues/{slot}.ogg", "fileID": audio["fileID"],
                       "uuid": str(UUID(guid)), "sha256": digest(raw)})
    return dict(table, schemaVersion=1, inventorySHA256=digest(inventory), sounds=sounds)


def attach_variants(level, sounds):
    nodes = level["nodes"]
    by_id = {node["id"]: node for node in nodes}
    if any(sound["node"] in by_id for sound in sounds):
        raise ValueError("Room already contains timer variants")
    for sound in sounds:
        default = "timer-" + sound["cue"].split("_")[0].lower()
        volume = by_id[default]["components"]["azure.audio-source"]["data"]["volume"]
        component = {"type": "azure.audio-source", "version": 1,
                     "data": {"asset": "assets:/" + sound["asset"], "volume": volume}}
        nodes.append({"id": sound["node"], "components": {"azure.audio-source": component}})


def build(room, output, source, inventory):
    room = room.resolve(strict=True)
    output = output.resolve()
    if output.is_relative_to(room):
        raise ValueError("The timer sound output must be outside the input room")
    if output.exists():
        raise FileExistsError(output)
    project = room / "project.azureproject"
    descriptor = json.loads(project.read_bytes())
    mount, = (row for row in descriptor["mounts"] if row["name"] == "assets")
    if mount["path"] != "assets":
        raise ValueError("The room must mount its assets directory")
    base_level = room / "assets/room.azurelevel"
    level = json.loads(base_level.read_bytes())
    receipt = timer_plan(source, inventory)
    attach_variants(level, receipt["sounds"])
    with tempfile.TemporaryDirectory(prefix="room-timer-cues-", dir=output.parent) as folder:
        staged = Path(folder) / "room"
        assets = staged / "assets"
        shutil.copytree(room / "assets", assets, copy_function=copy_room_asset, ignore=shutil.ignore_patterns(".azure"))
        shutil.copyfile(project, staged / project.name)
        for prior in room.glob("*-receipt.json"):
            shutil.copyfile(prior, staged / prior.name)
        stage_live_audio_media(source, receipt["sounds"], assets)
        level_path = assets / base_level.name
        write(level_path, level)
        receipt.update(inputLevelSHA256=digest(base_level), roomSHA256=digest(level_path))
        write(staged / "timer-sound-receipt.json", receipt)
        publish_directory(staged, output)
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("room", "output", "source", "inventory"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    build(args.room, args.output, args.source, args.inventory)


if __name__ == "__main__":
    main()
