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
from tools.npr_room.window_scene_inventory import ROOM_SCENE


ASSETS = Path(__file__).resolve().parents[2] / "examples/npr-room/assets"
NODE = "room-buy-item"
ACTIONS = ("ambient-purchase-confirm", "ambient-purchase-cancel")
CITY_ACTIONS = ("ambient-city-window", "ambient-city-combined")
ANCHOR = '<span id="ambient-status"></span>'
CONTROLS = '''
<span id="ambient-purchase-balance" style="display: block;"></span>
<div id="ambient-purchase" style="display: none;">
<span id="ambient-purchase-price" style="display: block;"></span>
<button id="ambient-purchase-confirm" data-action="ambient-purchase-confirm">Buy</button>
<button id="ambient-purchase-cancel" data-action="ambient-purchase-cancel">Cancel</button>
</div>
'''
CITY_CONTROLS = '''
<div id="ambient-city-controls" style="display: none;">
<button id="ambient-city-window" data-action="ambient-city-window">Show window view</button>
<button id="ambient-city-combined" data-action="ambient-city-combined">Turn on environment</button>
</div>
'''


def system_sound(source, sound_name):
    master = "Assets/App/BulbulAssets/MasterData/ScriptableObjects/SystemSeMaster.asset"
    enum = "Assets/Scripts/Assembly-CSharp/Bulbul/SystemSeType.cs"
    code, = (number for number, name in enum_values((source / enum).read_text(encoding="utf-8-sig")).items()
             if name == sound_name)
    clip, = (row["AudioClip"] for row in UnityScene(source / master).read(11400000)["SystemSes"]
             if row["SeSound"] == code)
    relative, = group_paths_by_guid(asset_index(source))[clip["guid"]]
    raw = source / relative
    if raw.suffix.lower() != ".ogg":
        raise ValueError(f"Expected original {sound_name} cue as Ogg")
    return {"source": relative, "asset": "system/" + raw.name, "sha256": digest(raw), "fileID": clip["fileID"],
            "uuid": str(UUID(clip["guid"])), "cue": code, "masterSHA256": digest(source / master),
            "enumSHA256": digest(source / enum)}


def city_binding(inputs, source, level, scenes, receipt):
    cases = json.loads(scenes.read_bytes())
    imported = json.loads(receipt.read_bytes())
    case = cases["cases"][str(imported["case"])]
    renderer, = (value for value in case["renderers"] if value["activeWhenSelected"] and value["kind"] == "mesh")
    expected_node = f"window-city-{renderer['transform']}"
    if (case["name"] != "City" or case["initialActive"] or renderer["component"] != imported["renderer"]
            or imported["node"] != expected_node or cases["roomSceneSHA256"] != digest(source / ROOM_SCENE)):
        raise ValueError("City scene and mesh receipt describe different source branches")
    node, = (node for node in level["nodes"] if node["id"] == expected_node)
    resource, = (asset for asset in level["resources"] if asset["id"] == node["resourceId"])
    if resource["asset"] != "assets:/window-city.glb" or digest(inputs / "window-city.glb") != imported["assetSHA256"]:
        raise ValueError("City room mesh differs from the import receipt")
    node["components"]["azure.renderable"] = {"type": "azure.renderable", "version": 1, "data": {"visible": False}}
    return {"node": expected_node, "sceneCase": imported["case"], "renderer": renderer["component"],
            "catalogSHA256": digest(scenes), "importReceiptSHA256": digest(receipt),
            "meshSHA256": imported["assetSHA256"]}


def build(room, output, source, unlock_catalog, window_scenes=None, city_receipt=None):
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
    sound = system_sound(source, "BuyItem")
    program = [
        "local wallet_catalog = " + lua_value(offers),
        (ASSETS / "room-wallet.lua").read_text(encoding="utf-8"),
        access_config(assets, catalog, names),
        (ASSETS / "ambient-access.lua").read_text(encoding="utf-8"),
        ambient_config(assets),
    ]
    descriptor = json.loads((room / "project.azureproject").read_bytes())
    level = json.loads((inputs / "room.azurelevel").read_bytes())
    actions = descriptor["runtime"]["input"]
    if (window_scenes is None) != (city_receipt is None):
        raise ValueError("City switching requires both the locked scene catalog and mesh receipt")
    view = city_binding(inputs, source, level, window_scenes, city_receipt) if window_scenes else None
    ui_actions = ACTIONS + (CITY_ACTIONS if view else ())
    if set(ui_actions) & actions.keys() or NODE in {node["id"] for node in level["nodes"]}:
        raise ValueError("The room already has purchase controls")
    actions.update(dict.fromkeys(ui_actions, []))
    level["nodes"].append({"id": NODE, "components": {"azure.audio-source": {
        "type": "azure.audio-source", "version": 1, "data": {"asset": "assets:/" + sound["asset"], "volume": 0.5}
    }}})
    ui = (inputs / "timer.rml").read_text(encoding="utf-8")
    timer = (inputs / "pomodoro.lua").read_text(encoding="utf-8")
    if view:
        program.extend((
            "local city_window_node = " + lua_value(view["node"]),
            "local city_sound_id = " + lua_value(enabled["City"]),
            (ASSETS / "room-window.lua").read_text(encoding="utf-8"),
        ))
    program.append((ASSETS / "ambient.lua").read_text(encoding="utf-8"))
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
        controls = CONTROLS + (CITY_CONTROLS if view else "")
        (target / "timer.rml").write_text(ui.replace(ANCHOR, controls + ANCHOR), encoding="utf-8", newline="\n")
        (target / "pomodoro.lua").write_text(timer.replace(points, " · Level %d · Points earned %d"),
                                           encoding="utf-8", newline="\n")
        (target / "ambient.lua").write_text("\n".join(program), encoding="utf-8", newline="\n")
        stage_live_audio_media(source, [sound], target)
        result = {"schemaVersion": 1, "sourceUnlocksSHA256": digest(unlock_catalog), "buySound": sound,
                  "environmentPrices": {name: offer[0] for name, offer in offers.items()},
                  "ambientOffers": enabled, "roomSHA256": digest(target / "room.azurelevel")}
        if view:
            result["cityWindow"] = view
        write(staged / "purchase-room-receipt.json", result)
        publish_directory(staged, output)
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("room", "output", "source", "unlock-catalog"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--window-scenes", type=Path)
    parser.add_argument("--city-receipt", type=Path)
    args = parser.parse_args()
    print(json.dumps(build(args.room, args.output, args.source, args.unlock_catalog, args.window_scenes, args.city_receipt)))
