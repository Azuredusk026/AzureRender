"""Connect the sampled body states and timed transitions to the opening story."""

import argparse
import json
from pathlib import Path
import shutil
import tempfile

from tools.npr_room.build_city_window import json_file
from tools.npr_room.build_story_book_room import FACTORY, START, skip_capture_orphans
from tools.npr_room.build_story_room import EXAMPLE
from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest
from tools.npr_room.room_geometry import copy_room_asset
from tools.npr_room.story_body_timeline import compile_timeline
from tools.npr_room.story_chapters import lua_value


GRAPH = "heroine/humanoid-motion.json"
BOOKS = "local story_book_layouts = "
CALL = """        roomOpening = new_room_opening(roomLanguage, function()
            local progress = workCalendar and workCalendar:progress()
            return progress and progress[1] or 1
        end)"""


def integrate_timer(script, states):
    anchors = (START, BOOKS, FACTORY, CALL, ", level, new_story_book_layout(self, story_book_layouts, ")
    if any(script.count(anchor) != 1 for anchor in anchors):
        raise ValueError("Expected the authored story body and book hooks")
    start, end = script.index(START), script.index(BOOKS)
    factory_start = script.index(FACTORY)
    factory_end = script.index("\nend\n", factory_start)
    factory = script[factory_start:factory_end]
    if not factory.endswith("))"):
        raise ValueError("Expected the opening factory to construct its book controller")
    factory = factory.replace(FACTORY, FACTORY[:-1] + ", setMotion)")
    factory = factory[:-1] + ", new_story_body(setMotion, story_body_states))"
    body = (EXAMPLE / "story-body.lua").read_text(encoding="utf-8")
    story = (EXAMPLE / "story-room.lua").read_text(encoding="utf-8")
    config = "local story_body_states = " + lua_value(states)
    rest = script[end:factory_start] + factory + script[factory_end:]
    merged = script[:start] + "\n".join((body, story, config, rest))
    return merged.replace(CALL, CALL[:-1] + ", setHeroineState)")


def stage(room, catalog_path, chapters_path, output):
    graph = json_file(room / "assets" / GRAPH)
    source = json_file(catalog_path)
    assembled = json_file(room / "opening-body-receipt.json")
    chapter_digest = digest(chapters_path)
    if assembled["catalogSHA256"] != digest(catalog_path) or source["sourceSHA256"]["openingChapters"] != chapter_digest:
        raise ValueError("Opening body route must use the staged source clips and chapter commands")
    for part, record in assembled["parts"].items():
        if digest(room / "assets/heroine" / (part + ".glb")) != record["sha256"]:
            raise ValueError(f"Opening body part changed: {part}")
    timeline = compile_timeline(graph, source, assembled)
    state_clips = {state["name"]: state["clip"] for state in timeline["states"]}
    chapters = json_file(chapters_path)["chapters"]
    commands = {str(row["bodyMotion"]) for chapter in chapters for row in chapter["commands"]
                if row["command"] in ("Text", "ChangeMotion") and row["bodyMotion"] != -1}
    states = {key: "story-" + item["states"][0]["state"] for key, item in source["motions"].items()}
    if commands != states.keys() or not set(states.values()) <= state_clips.keys() or "0" not in states:
        raise ValueError("Body commands and sampled story state entries differ")

    with tempfile.TemporaryDirectory(prefix="story-body-route-", dir=output.parent) as directory:
        project = Path(directory) / "project"
        assets = project / "assets"
        shutil.copytree(room / "assets", assets, copy_function=copy_room_asset, ignore=skip_capture_orphans)
        for path in [room / "project.azureproject", *room.glob("*-receipt.json")]:
            shutil.copyfile(path, project / path.name)
        write(assets / GRAPH, timeline)
        script = assets / "pomodoro.lua"
        code = integrate_timer(script.read_text(encoding="utf-8"), states)
        script.write_text(code, encoding="utf-8", newline="\n")
        startup = json_file(project / "opening-story-room-receipt.json")
        startup["scriptSHA256"] = digest(script)
        write(project / "opening-story-room-receipt.json", startup)
        receipt = {
            "schemaVersion": 1, "chaptersSHA256": chapter_digest, "catalogSHA256": digest(catalog_path),
            "clipEntries": {key: state_clips[state] for key, state in states.items()}, "stateEntries": states,
            "timedTransitions": timeline["timeline"], "graphSHA256": digest(assets / GRAPH),
            "scriptSHA256": startup["scriptSHA256"], "parts": list(assembled["parts"]),
        }
        write(project / "opening-story-motion-receipt.json", receipt)
        publish_directory(project, output)
    return receipt


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for arg in ("room", "output", "catalog", "chapters"):
        parser.add_argument("--" + arg, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(stage(args.room, args.catalog, args.chapters, args.output)))
