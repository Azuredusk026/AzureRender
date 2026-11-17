"""Stage a source Animator and face bindings for an offline Unity layer trace."""

import argparse
import json
from pathlib import Path
import shutil

from tools.build_face_sdf import parse_glb
from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest


def stage(source, project, graph, chapter_file, model, body_morphs=None, additive_overlays=False):
    controller = json.loads(graph.read_bytes())
    chapters = json.loads(chapter_file.read_bytes())["chapters"]
    numbers = {row["faceMotion"] for chapter in chapters for row in chapter["commands"]
               if "faceMotion" in row and row["faceMotion"] >= 0} | {18}
    wanted = {controller["source"]: controller["sourceSHA256"]}
    routes = {}
    state_layers = {row["m_Name"]: row for row in controller["layers"]}
    layer_names = ("Facial", "Talk", "LookPlayer", "plankshake") if additive_overlays else ("Facial", "Talk", "LookPlayer")
    for name in layer_names:
        layer = state_layers[name]
        machine = controller["machines"][str(layer["m_StateMachine"]["fileID"])]
        records = {str(item["m_State"]["fileID"]): controller["states"][str(item["m_State"]["fileID"])]
                   for item in machine["m_ChildStates"]}
        if name == "Facial":
            for pointer in machine["m_AnyStateTransitions"]:
                edge = controller["stateTransitions"][str(pointer["fileID"])]
                condition, = edge["m_Conditions"]
                if (condition["m_ConditionEvent"] != "Facial" or condition["m_ConditionMode"] != 6
                        or not edge["m_HasFixedDuration"] or edge["m_HasExitTime"]):
                    raise ValueError("Expected the original Facial integer")
                number = condition["m_EventTreshold"]
                if number not in numbers:
                    continue
                state = records[str(edge["m_DstState"]["fileID"])]
                routes[number] = {"name": state["m_Name"], "blend": edge["m_TransitionDuration"]}
            selected = {row["name"] for row in routes.values()}
            records = {key: value for key, value in records.items() if value["m_Name"] in selected}
        for state in records.values():
            motion = state["m_Motion"]
            path, = controller["externalAssets"][motion["guid"]]
            wanted[path] = digest(source / path)
    if set(routes) != numbers:
        raise ValueError("Opening expressions do not resolve to the source graph")

    bodies = {}
    if body_morphs:
        bodies = json.loads(body_morphs.read_bytes())["clips"]
        if any(row["relative"] in wanted for row in bodies.values()):
            raise ValueError("Body probe inputs overlap the facial layer")
        wanted.update({row["relative"]: row["sha256"] for row in bodies.values()})

    sampler = Path(__file__).with_name("unity") / "StoryFaceControllerProbe.cs"
    assets = project / "Assets"
    for relative, sha in wanted.items():
        original = (source / relative).resolve(strict=True)
        if not original.is_relative_to(source / "Assets") or digest(original) != sha:
            raise ValueError(f"Changed source animation: {original}")
        for suffix in ("", ".meta"):
            path = Path(str(original) + suffix)
            target = project / (relative + suffix)
            target.parent.mkdir(parents=True, exist_ok=True)
            if target.exists():
                if digest(target) != digest(path):
                    raise ValueError(f"Staged animation differs: {target}")
            else:
                shutil.copyfile(path, target)
    shutil.copyfile(sampler, assets / "Editor/StoryFaceControllerProbe.cs")

    gltf, _ = parse_glb(model)
    mesh, = gltf["meshes"]
    names, weights = mesh["extras"]["targetNames"], mesh["weights"]
    if len(names) != 57 or len(set(names)) != 57 or len(weights) != len(names):
        raise ValueError("Expected all source face bindings")
    config = {"names": names, "defaults": [weight * 100 for weight in weights],
              "bodyClips": [row["relative"] for row in bodies.values()]}
    write(project / "story-face-probe.json", config)
    receipt = {"schemaVersion": 1, "graphSHA256": digest(graph), "chaptersSHA256": digest(chapter_file),
               "meshSHA256": digest(model), "samplerSHA256": digest(sampler), "assets": wanted,
               "facialRoutes": routes}
    write(project / "story-face-source-receipt.json", receipt)
    return receipt


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    names = ("source", "project", "graph", "chapters", "model")
    for name in names:
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--body-morphs", type=Path)
    parser.add_argument("--additive-overlays", action="store_true")
    args = vars(parser.parse_args())
    print(json.dumps(stage(*(args[arg].resolve(strict=True) for arg in names),
                           body_morphs=args["body_morphs"], additive_overlays=args["additive_overlays"])))
