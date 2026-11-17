"""Assemble the original opening story in the room's normal entry scene."""

import argparse
import json
from pathlib import Path
import shutil
import tempfile

from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest
from tools.npr_room.room_geometry import copy_room_asset
from tools.npr_room.room_language import FIELDS
from tools.npr_room.story_chapters import lua_value, translations


EXAMPLE = Path(__file__).resolve().parents[2] / "examples/npr-room/assets"
PREFIX = "opening-story-"
CONTROLS = ("start", "log", "next", "auto", "heroine", *(f"option-{i}" for i in range(1, 5)),
            *(f"replay-{i}" for i in range(1, 3)))
SOURCES = (
    ("opening-story-voice", "azure.audio-source", {"volume": .5}),
    ("opening-story-music", "azure.audio-source", {"volume": 0, "loop": True}),
    ("opening-story-noise", "azure.screen-noise", {}),
)


def configuration(room, chapters, inventory):
    story = json.loads(chapters.read_bytes())
    dialogue = json.loads((room / "opening-dialogue-receipt.json").read_bytes())
    if dialogue["sourceSHA256"] != digest(chapters) or story["sourceSHA256"]["inventory"] != digest(inventory):
        raise ValueError("The opening story and localized dialogue must share the same source")
    titles = {row["arguments"][0] for chapter in story["chapters"] for row in chapter["commands"]
              if row["command"] in ("PlayBGM", "TalkLogOnlyPlayBGM")}
    playlist = json.loads((room / "music-receipt.json").read_bytes())["tracks"]
    tracks = {}
    for title in sorted(titles):
        track, = (track for track in playlist if track["title"] == title)
        if digest(room / "assets" / track["asset"]) != track["sha256"]:
            raise ValueError(f"Original music asset differs: {title}")
        tracks[title] = "assets:/" + track["asset"]
    for asset, expected in dialogue["voiceSHA256"].items():
        if digest(room / "assets" / asset) != expected:
            raise ValueError(f"Original voice asset differs: {asset}")
    strings = {row["ID"]: row for row in json.loads(inventory.read_bytes())["localization"]}
    labels = {key: translations(strings[label]) for key, label in
              (("log", "ui_story_title"), ("auto-label", "ui_scenario_auto"))}
    return {"room_story_chapters": story["chapters"], "opening_tracks": tracks, "opening_tags": list(FIELDS),
            "opening_labels": labels}


def markup(base):
    style, controls = (EXAMPLE / "story-player.rml").read_text(encoding="utf-8").split("</style>", 1)
    room_style, room_controls = (EXAMPLE / "story-room.rml").read_text(encoding="utf-8").split("</style>", 1)
    if base.count("</style>") != 1 or base.count("</body>") != 1 or PREFIX in base:
        raise ValueError("Expected a room document without opening controls")
    return base.replace("</style>", style + room_style + "</style>").replace("</body>", room_controls + controls + "</body>")


def stage(room, output, chapters, inventory):
    config = configuration(room, chapters, inventory)
    descriptor = json.loads((room / "project.azureproject").read_bytes())
    actions = {PREFIX + control: [] for control in CONTROLS}
    if descriptor["runtime"]["input"].keys() & actions.keys():
        raise ValueError("The room already has opening actions")
    descriptor["runtime"]["input"].update(actions)
    level = json.loads((room / "assets/room.azurelevel").read_bytes())
    if {node["id"] for node in level["nodes"]} & {name for name, _, _ in SOURCES}:
        raise ValueError("The room already has opening sources")
    level["nodes"].extend({"id": name, "components": {kind: {"type": kind, "version": 1, "data": data}}}
                          for name, kind, data in SOURCES)
    timer = (room / "assets/pomodoro.lua").read_bytes().replace(b"\r\n", b"\n")
    marker = b'local slot = "pomodoro-settings"\n'
    if timer.count(marker) != 1 or b"local room_language_config =" not in timer or b"local timer_voice_program =" not in timer:
        raise ValueError("The opening story requires the localized timer and room voices")
    prefix, _ = timer.split(marker)
    lua = "\n".join(f"local {name} = {lua_value(value)}" for name, value in config.items()).encode()
    factory = b'''
local function new_room_opening(roomLanguage, level)
    return new_story_room(self, room_story_chapters, opening_tracks, opening_caption_speeds, opening_labels,
        function() return roomLanguage:text(opening_tags) end, level)
end
'''
    script = b"\n".join((prefix, lua, (room / "assets/opening-dialogue-program.lua").read_bytes(),
                         *((EXAMPLE / file).read_bytes() for file in ("story-progress.lua", "story-reader.lua",
                           "story-player.lua", "story-music.lua", "story-noise.lua", "story-room.lua")), factory,
                         (EXAMPLE / "pomodoro.lua").read_bytes()))
    content = markup((room / "assets/timer.rml").read_text(encoding="utf-8"))
    with tempfile.TemporaryDirectory(prefix="story-room-", dir=output.parent) as parent:
        project = Path(parent) / "project"
        assets = project / "assets"
        shutil.copytree(room / "assets", assets, copy_function=copy_room_asset, ignore=shutil.ignore_patterns(".azure"))
        for receipt in room.glob("*-receipt.json"):
            shutil.copyfile(receipt, project / receipt.name)
        write(project / "project.azureproject", descriptor)
        write(assets / "room.azurelevel", level)
        (assets / "pomodoro.lua").write_bytes(script)
        (assets / "timer.rml").write_text(content, encoding="utf-8", newline="\n")
        report = {"schemaVersion": 1, "chaptersSHA256": digest(chapters), "inventorySHA256": digest(inventory),
                  "startupScene": descriptor["startupScene"], "roomScene": "assets:/room.azurelevel",
                  "levelSHA256": digest(assets / "room.azurelevel"), "scriptSHA256": digest(assets / "pomodoro.lua"),
                  "documentSHA256": digest(assets / "timer.rml"), "tracks": config["opening_tracks"]}
        write(project / "opening-story-room-receipt.json", report)
        publish_directory(project, output)
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("room", "chapters", "inventory", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(stage(args.room, args.output, args.chapters, args.inventory)))
