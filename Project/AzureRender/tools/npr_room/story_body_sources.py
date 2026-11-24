"""Resolve the original body-motion states used by the opening chapters."""

import argparse
import json
from pathlib import Path
import re

from tools.npr_room.reference_inventory import digest
from tools.npr_room.timer_voice import ANIMATION_ENUM, clip_voice_events


OPENING_ROUTES = {
    0: ("Base001",),
    1: ("Motion1_Start_FrustrationRight", "Base001_Motion1_FrustrationRight", "Motion1_End_FrustrationRight"),
    3: ("Motion3_Start_PressHands", "Base001_Motion3_PressHands", "Motion3_End_PressHands"),
    5: ("Motion5_Start_Shy", "Base001_Motion5_Shy", "Motion5_Base001_Story"),
    12: ("Base001_Motion12_Start_Nod", "Motion12_Base001_Story"),
    15: ("Base001_Motion15_Start_Introduce", "Motion15_Loop_Introduce", "Motion15_End_Introduce"),
    401: ("GameStart_End",),
    1000: ("anim_story_subbase001_base_start", "Story_SubBase001", "anim_story_subbase001_base_end"),
    1003: ("Story_SubBase001_Fun", "Story_SubBase001"),
}


def definition(source, chapter_path, graph_path):
    body_enum = source / ANIMATION_ENUM
    block = body_enum.read_text(encoding="utf-8-sig").split("public enum AnimationType", 1)[1].split("}", 1)[0]
    names = {int(number): name for name, number in re.findall(r"(\w+)\s*=\s*(-?\d+)", block)}
    scene = json.loads(chapter_path.read_bytes())
    used = {row["bodyMotion"] for chapter in scene["chapters"] for row in chapter["commands"]
            if row.get("bodyMotion", -1) >= 0}
    if used != OPENING_ROUTES.keys():
        raise ValueError(f"Opening story has different body motions: {sorted(used)}")
    graph = json.loads(graph_path.read_bytes())
    if digest(source / graph["source"]) != graph["sourceSHA256"]:
        raise ValueError("Opening story requires the locked original animation controller")
    states = graph["states"]
    routes, clips = {}, {}
    for identity, route in OPENING_ROUTES.items():
        if names[identity] not in route:
            raise ValueError(f"Story motion {identity} does not include its authored selector state")
        t = []
        for name in route:
            matching = {key: item for key, item in states.items() if item["m_Name"] == name}
            pointer, = {(item["m_Motion"]["fileID"], item["m_Motion"]["guid"]) for item in matching.values()}
            clip_path, = graph["externalAssets"][pointer[1]]
            location = (source / clip_path).resolve(strict=True)
            if pointer[0] != 7400000 or location.suffix != ".anim" or not location.is_relative_to(source.resolve()):
                raise ValueError(f"Original story body motion requires a local AnimationClip: {name}")
            if any(row["m_Speed"] != 1 or row["m_SpeedParameterActive"] for row in matching.values()):
                raise ValueError(f"Story body motion has a different authored playback speed: {name}")
            clips[clip_path] = {"sha256": digest(location), "guid": pointer[1], **clip_voice_events(location)}
            identities = []
            for key, item in matching.items():
                exits = [graph["stateTransitions"][str(edge["fileID"])] for edge in item["m_Transitions"]]
                identities.append({"id": key, "outgoing": exits})
            t.append({"state": name, "clip": clip_path, "variants": identities})
        routes[str(identity)] = {"selector": names[identity], "states": t}
    return {"schemaVersion": 1, "sourceSHA256": {"openingChapters": digest(chapter_path), "graph": digest(graph_path),
            "animationEnum": digest(body_enum)}, "motions": routes, "clips": clips}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source", "chapters", "graph", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    project = definition(args.source, args.chapters, args.graph)
    with args.output.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(project, stream, sort_keys=True, separators=(",", ":"))
        stream.write("\n")
    print(json.dumps({"motions": len(project["motions"]), "clips": len(project["clips"]), "sha256": digest(args.output)}))
