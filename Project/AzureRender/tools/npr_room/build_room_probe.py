"""Assemble the local P1 probe from an imported desktop furniture project."""

import argparse
import json
from pathlib import Path
import shutil

from reference_inventory import asset_index, group_paths_by_guid


CUES = {"timer-start": (0, 0.5), "timer-pause": (6, 1.0), "timer-finish": (3, 0.5)}


def component(type_id, data):
    return {"type": type_id, "version": 1, "data": data}


def make_probe(example, source, furniture, output):
    import re

    master = source / "Assets/App/BulbulAssets/MasterData/ScriptableObjects/TimerSeMaster.asset"
    cues = dict(re.findall(r"(?m)^  - SeSound: (\d+)\n    AudioClip: \{fileID: \d+, guid: ([a-f0-9]{32}),", master.read_text(encoding="utf-8")))
    by_guid = group_paths_by_guid(asset_index(source))
    audio = {}
    for slot, (cue, volume) in CUES.items():
        relative, = by_guid[cues[str(cue)]]
        original = source / relative
        if original.suffix.lower() != ".ogg":
            raise ValueError(f"Expected OGG for {slot}")
        audio[slot] = (original, volume)

    level = json.loads((furniture / "assets/furniture.azurelevel").read_text(encoding="utf-8"))
    import_record = level["sceneImports"]["recovery-furniture"]
    chair_id = import_record["nodeMap"]["transform-7570"]
    chair, = [node for node in level["nodes"] if node["id"] == chair_id]
    chair["components"]["azure.animator"] = component(
        "azure.animator", {"asset": "assets:/chair-motion.json", "state": "window"}
    )
    level["id"] = "room"
    level["nodes"].append({
        "id": "room-timer",
        "components": {
            "azure.script": component("azure.script", {"asset": "assets:/pomodoro.lua", "enabled": True}),
            "azure.game-ui": component("azure.game-ui", {"asset": "assets:/timer.rml", "enabled": True}),
        },
    })
    for slot, (_, volume) in audio.items():
        level["nodes"].append({
            "id": slot,
            "components": {
                "azure.audio-source": component("azure.audio-source", {"asset": f"assets:/{slot}.ogg", "volume": volume}),
            },
        })

    output.mkdir()
    shutil.copy2(example / "project.azureproject", output)
    assets = output / "assets"
    shutil.copytree(example / "assets", assets)
    shutil.copytree(furniture / "assets/exchange", assets / "exchange",
                    ignore=shutil.ignore_patterns("dangling.azuresceneexchange*"))
    for slot, (original, _) in audio.items():
        shutil.copyfile(original, assets / f"{slot}.ogg")
    (assets / "room.azurelevel").write_text(json.dumps(level, indent=2) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--furniture", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    example = Path(__file__).resolve().parents[2] / "examples/npr-room"
    make_probe(example, args.source, args.furniture, args.output)


if __name__ == "__main__":
    main()
