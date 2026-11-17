"""Prepare the native opening dialogue caption and original voice sources."""

import argparse
import json
import math
from pathlib import Path
import shutil
import tempfile

from tools.npr_room.build_work_calendar import PARAMETER_ASSET
from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest
from tools.npr_room.room_geometry import copy_room_asset, stage_live_audio_media
from tools.npr_room.skinned_mesh_export import UnityScene
from tools.npr_room.story_chapters import lua_value


SUFFIXES = {"Ja": "Jp", "En": "En", "ZhHans": "CS", "ZhHant": "CT", "Pt": "Pt", "Ko": "Ko", "Ru": "Ru"}


def talk_speed(source):
    asset = source / PARAMETER_ASSET
    values = UnityScene(asset).read(11400000)["TalkSpeedData"]
    speeds = {language: values["TalkTextWaitSeconds" + suffix] for language, suffix in SUFFIXES.items()}
    if any(not math.isfinite(value) or value <= 0 for value in speeds.values()):
        raise ValueError("Expected finite original caption intervals for every Windows language")
    return speeds, digest(asset)


def stage(room, output, source, chapter_path):
    original = json.loads(chapter_path.read_bytes())
    body = json.loads((room / "opening-body-receipt.json").read_bytes())
    language = json.loads((room / "language-receipt.json").read_bytes())
    if language["storySHA256"] != original["sourceSHA256"]["inventory"] or not body["motions"]:
        raise ValueError("Opening dialogue requires the original language and body assets")
    rows = [row for chapter in original["chapters"] for row in chapter["commands"] if row["command"] == "Text"]
    if not rows or any("<" in text for row in rows for text in row["translations"].values()):
        raise ValueError("Opening dialogue requires plain source localization")
    speeds, parameters_hash = talk_speed(source)
    example = Path(__file__).resolve().parents[2] / "examples/npr-room/assets"
    with tempfile.TemporaryDirectory(prefix="story-dialogue-", dir=output.parent) as parent:
        project = Path(parent) / "project"
        assets = project / "assets"
        shutil.copytree(room / "assets", assets, copy_function=copy_room_asset, ignore=shutil.ignore_patterns(".azure"))
        for file in room.iterdir():
            if file.is_file() and (file.name == "project.azureproject" or file.name.endswith("-receipt.json")):
                shutil.copyfile(file, project / file.name)
        missing = []
        for media in original["media"]:
            target = assets / media["asset"]
            if not target.exists():
                missing.append(media)
            elif digest(target) != media["sha256"]:
                raise ValueError(f"Original opening voice differs: {media['guid']}")
        stage_live_audio_media(source, missing, assets)
        configuration = "local opening_caption_speeds = " + lua_value(speeds) + "\n"
        (assets / "opening-dialogue-program.lua").write_bytes(configuration.encode() + (example / "story-dialogue.lua").read_bytes())
        receipt = {"schemaVersion": 1, "sourceSHA256": digest(chapter_path), "parameterSHA256": parameters_hash,
                   "voiceSHA256": {clip["asset"]: clip["sha256"] for clip in original["media"]}, "speeds": speeds,
                   "lineCount": len(rows), "languageDefault": language["default"]}
        write(project / "opening-dialogue-receipt.json", receipt)
        publish_directory(project, output)
    return receipt


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("room", "output", "source", "chapters"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    data = stage(args.room, args.output, args.source, args.chapters)
    print(json.dumps({"lines": data["lineCount"], "originalVoiceAssets": len(data["voiceSHA256"]), "speeds": data["speeds"]}))
