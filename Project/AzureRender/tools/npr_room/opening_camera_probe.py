"""Stage the Windows opening camera controller and its minimal source hierarchy."""

import argparse
import json
from pathlib import Path
import re
import shutil

from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest
from tools.npr_room.skinned_mesh_export import UnityScene
from tools.npr_room.window_scene_inventory import ROOM_SCENE


def stage(source, project):
    scene_path = source / ROOM_SCENE
    scene = UnityScene(scene_path)
    manager = scene.read(21884)
    camera = scene.read(manager["_cam"]["fileID"])
    animator = scene.read(manager["_animator"]["fileID"])
    anchor = manager["_camTransform"][0]["fileID"]
    transform_id = lambda entity: next(
        ref["component"]["fileID"] for ref in scene.read(entity)["m_Component"]
        if scene.documents[ref["component"]["fileID"]][0] == 4)
    root = transform_id(animator["m_GameObject"]["fileID"])
    leaf = transform_id(camera["m_GameObject"]["fileID"])
    relative = ("Assets/AnimatorController/Cameras_Animator.controller", "Assets/AnimationClip/anim_startcamera.anim")
    for path in relative:
        path = source / path
        if not path.is_file():
            raise ValueError(f"Missing source camera asset: {path}")
    def guid(path):
        return re.search(r"(?m)^guid: ([0-9a-f]{32})$", (source / (path + ".meta")).read_text())[1]
    if (animator["m_Controller"]["guid"] != guid(relative[0])
            or not camera["m_Enabled"] or animator["m_Enabled"]):
        raise ValueError("Expected the original inactive Windows camera controller")
    controller = UnityScene(source / relative[0])
    state, = [controller.read(identity) for identity, data in controller.documents.items()
              if data[0] == 1102 and controller.read(identity)["m_Name"] == "anim_startcamera_pc"]
    if (state["m_Motion"]["guid"] != guid(relative[1]) or state["m_Motion"]["fileID"] != 7400000
            or state["m_Speed"] != 1 or state["m_Transitions"]):
        raise ValueError("Unexpected Windows opening camera state")
    nodes, indices = [], {}
    def add(identity):
        if not identity:
            return -1
        if identity not in indices:
            data = scene.read(identity)
            parent = add(data["m_Father"]["fileID"])
            indices[identity] = len(nodes)
            nodes.append({"parent": parent, "name": scene.read(data["m_GameObject"]["fileID"])["m_Name"],
                          "position": data["m_LocalPosition"], "rotation": data["m_LocalRotation"],
                          "scale": data["m_LocalScale"]})
        return indices[identity]
    config = {"root": add(root), "camera": add(leaf), "anchor": add(anchor),
              "state": state["m_Name"], "controller": relative[0], "clip": relative[1], "nodes": nodes}
    sampler = Path(__file__).with_name("unity") / "OpeningCameraProbe.cs"
    files = {entry: source / entry for path in relative for entry in (path, path + ".meta")}
    files["Assets/Editor/OpeningCameraProbe.cs"] = sampler
    for name, path in files.items():
        target = project / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(path, target)
    config_path = project / "opening-camera-probe.json"
    write(config_path, config)
    receipt = {"schemaVersion": 1, "sceneSHA256": digest(scene_path), "probeSHA256": digest(config_path),
               "files": {name: digest(path) for name, path in files.items()},
               "sourceIDs": {"manager": 21884, "camera": leaf, "anchor": anchor, "animator": root}}
    write(project / "opening-camera-source-receipt.json", receipt)
    return receipt


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source", "project"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(stage(args.source.resolve(strict=True), args.project.resolve(strict=True))))
