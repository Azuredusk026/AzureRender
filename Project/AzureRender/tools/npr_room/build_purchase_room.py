"""Attach source-priced ambience purchases to a local Windows room."""

import argparse
import json
from pathlib import Path
import shutil
import tempfile
from uuid import UUID

from tools.npr_room.build_ambient_room import AMBIENT_ENUM, ambient_config, ambient_names
from tools.npr_room.build_unlock_room import access_config
from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.feature_matrix import enum_values
from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import asset_index, digest, group_paths_by_guid
from tools.npr_room.room_geometry import copy_room_asset, stage_live_audio_media
from tools.npr_room.skinned_mesh_export import UnityScene
from tools.npr_room.story_chapters import lua_value


ASSETS = Path(__file__).resolve().parents[2] / "examples/npr-room/assets"
NODE = "room-buy-item"
ACTIONS = ("ambient-purchase-confirm", "ambient-purchase-cancel")
ANCHOR = '<span id="ambient-status"></span>'
CONTROLS = '''
<span id="ambient-purchase-balance" style="display: block;"></span>
<div id="ambient-purchase" style="display: none;">
<span id="ambient-purchase-price" style="display: block;"></span>
<button id="ambient-purchase-confirm" data-action="ambient-purchase-confirm">Buy</button>
<button id="ambient-purchase-cancel" data-action="ambient-purchase-cancel">Cancel</button>
</div>
'''


def purchase_sound(source):
    master = "Assets/App/BulbulAssets/MasterData/ScriptableObjects/SystemSeMaster.asset"
    enum = "Assets/Scripts/Assembly-CSharp/Bulbul/SystemSeType.cs"
    code, = (number for number, name in enum_values((source / enum).read_text(encoding="utf-8-sig")).items()
             if name == "BuyItem")
    clip, = (row["AudioClip"] for row in UnityScene(source / master).read(11400000)["SystemSes"]
             if row["SeSound"] == code)
    relative, = group_paths_by_guid(asset_index(source))[clip["guid"]]
    raw = source / relative
    if raw.suffix.lower() != ".ogg":
        raise ValueError("Expected original BuyItem cue as Ogg")
    return {"source": relative, "asset": "system/" + raw.name, "sha256": digest(raw), "fileID": clip["fileID"],
            "uuid": str(UUID(clip["guid"])), "cue": code, "masterSHA256": digest(source / master),
            "enumSHA256": digest(source / enum)}


def build(room, output, source, unlock_catalog):
    inputs = room / "assets"
    report = json.loads((room / "unlock-receipt.json").read_bytes())
    assets = json.loads((room / "ambient-receipt.json").read_bytes())
    catalog = json.loads(unlock_catalog.read_bytes())
    enum = source / AMBIENT_ENUM
    if report["unlockCatalogSHA256"] != digest(unlock_catalog) or assets["enumSHA256"] != digest(enum):
        raise ValueError("The room and purchases must use the same environment source")
    names = ambient_names(enum)
    offers = {name: [row["cost"], row.get("expires", "")] for name, row in catalog["environment"].items()
              if row["policy"] == "purchase"}
    enabled = {names[row["id"]]: row["id"] for row in assets["sounds"] if row["loop"] and names[row["id"]] in offers}
    sound = purchase_sound(source)
    program = "\n".join((
        "local wallet_catalog = " + lua_value(offers),
        (ASSETS / "room-wallet.lua").read_text(encoding="utf-8"),
        access_config(assets, catalog, names),
        (ASSETS / "ambient-access.lua").read_text(encoding="utf-8"),
        ambient_config(assets),
        (ASSETS / "ambient.lua").read_text(encoding="utf-8"),
    ))
    descriptor = json.loads((room / "project.azureproject").read_bytes())
    level = json.loads((inputs / "room.azurelevel").read_bytes())
    actions = descriptor["runtime"]["input"]
    if set(ACTIONS) & actions.keys() or NODE in {node["id"] for node in level["nodes"]}:
        raise ValueError("The room already has purchase controls")
    actions.update(dict.fromkeys(ACTIONS, []))
    level["nodes"].append({"id": NODE, "components": {"azure.audio-source": {
        "type": "azure.audio-source", "version": 1, "data": {"asset": "assets:/" + sound["asset"], "volume": 0.5}
    }}})
    ui = (inputs / "timer.rml").read_text(encoding="utf-8")
    timer = (inputs / "pomodoro.lua").read_text(encoding="utf-8")
    points = " · Level %d · Points %d"
    if ui.count(ANCHOR) != 1 or timer.count(points) != 1:
        raise ValueError("The room needs its ambient menu and work rewards")
    with tempfile.TemporaryDirectory(prefix="room-purchases-", dir=output.parent) as temporary:
        staged = Path(temporary) / "project"
        target = staged / "assets"
        shutil.copytree(inputs, target, copy_function=copy_room_asset, ignore=shutil.ignore_patterns(".azure"))
        for receipt in room.glob("*-receipt.json"):
            shutil.copyfile(receipt, staged / receipt.name)
        write(staged / "project.azureproject", descriptor)
        write(target / "room.azurelevel", level)
        (target / "timer.rml").write_text(ui.replace(ANCHOR, CONTROLS + ANCHOR), encoding="utf-8", newline="\n")
        (target / "pomodoro.lua").write_text(timer.replace(points, " · Level %d · Points earned %d"),
                                           encoding="utf-8", newline="\n")
        (target / "ambient.lua").write_text(program, encoding="utf-8", newline="\n")
        stage_live_audio_media(source, [sound], target)
        result = {"schemaVersion": 1, "sourceUnlocksSHA256": digest(unlock_catalog), "buySound": sound,
                  "environmentPrices": {name: offer[0] for name, offer in offers.items()},
                  "ambientOffers": enabled, "roomSHA256": digest(target / "room.azurelevel")}
        write(staged / "purchase-room-receipt.json", result)
        publish_directory(staged, output)
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("room", "output", "source", "unlock-catalog"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(build(args.room, args.output, args.source, args.unlock_catalog)))
