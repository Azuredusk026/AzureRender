"""Compile direct timed transitions from the locked opening body controller."""

import argparse
import json
import math
from pathlib import Path

from tools.npr_room.reference_inventory import digest


def compile_timeline(graph, catalog, receipt):
    if graph["schemaVersion"] not in (1, 2):
        raise ValueError("Expected the assembled room's animation graph")
    clips = receipt["motions"]
    staged = {state["name"]: state["clip"] for state in graph["states"]}
    states, identities = {}, {}
    for motion in catalog["motions"].values():
        for state in motion["states"]:
            name = state["state"]
            if states.setdefault(name, state) != state:
                raise ValueError(f"Conflicting original motion state {name}")
            clip = clips[Path(state["clip"]).stem]
            if staged["story-" + name] != clip["clipIndex"] or not math.isfinite(clip["seconds"]) or clip["seconds"] <= 0:
                raise ValueError(f"Original motion duration and clip binding required: {name}")
            for variant in state["variants"]:
                identity = int(variant["id"])
                if identities.setdefault(identity, name) != name:
                    raise ValueError(f"Conflicting controller state identity {identity}")

    emitted = {}
    for name, state in states.items():
        duration = clips[Path(state["clip"]).stem]["seconds"]
        for variant in state["variants"]:
            for transition in variant["outgoing"]:
                target = identities.get(transition["m_DstState"]["fileID"])
                if (not target or transition["m_IsExit"] or transition["m_DstStateMachine"]["fileID"] or
                        transition["m_Conditions"] or not transition["m_HasExitTime"] or transition["m_Mute"]):
                    continue
                if not transition["m_HasFixedDuration"] or transition["m_InterruptionSource"] or transition["m_Solo"]:
                    raise ValueError(f"Expected fixed, non-interruptible direct transition: {name}")
                target_duration = clips[Path(states[target]["clip"]).stem]["seconds"]
                edge = {"from": "story-" + name, "to": "story-" + target,
                        "afterSeconds": duration * transition["m_ExitTime"],
                        "blendSeconds": transition["m_TransitionDuration"],
                        "startSeconds": target_duration * transition["m_TransitionOffset"]}
                if not all(math.isfinite(edge[field]) and edge[field] >= 0
                           for field in ("afterSeconds", "blendSeconds", "startSeconds")):
                    raise ValueError(f"Invalid original transition timing: {name}")
                if emitted.setdefault(name, edge) != edge:
                    raise ValueError(f"Multiple direct timed transitions from {name}")
    return {**graph, "schemaVersion": 3, "timeline": [emitted[name] for name in sorted(emitted)]}


def from_room(room, catalog_path):
    receipt = json.loads((room / "opening-body-receipt.json").read_bytes())
    if receipt["catalogSHA256"] != digest(catalog_path):
        raise ValueError("Opening motion catalog does not match the assembled room")
    return compile_timeline(json.loads((room / "assets/heroine/humanoid-motion.json").read_bytes()),
                            json.loads(catalog_path.read_bytes()), receipt)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("room", "catalog", "output"):
        parser.add_argument("--" + argument, type=Path, required=True)
    args = parser.parse_args()
    definition = from_room(args.room, args.catalog)
    with args.output.open("x", encoding="utf-8", newline="\n") as target:
        json.dump(definition, target, sort_keys=True, separators=(",", ":"))
        target.write("\n")
    print(json.dumps({"timelineSize": len(definition["timeline"]), "sha256": digest(args.output)}))
