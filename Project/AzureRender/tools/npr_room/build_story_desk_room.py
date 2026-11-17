"""Stage the source desk animator's endpoint poses and resting textbook skin."""

import argparse
import json
import math
from pathlib import Path
import shutil
import tempfile
from uuid import UUID

from tools.build_face_sdf import write_glb
from tools.npr_room.build_city_window import json_file
from tools.npr_room.build_story_book_room import START, skip_capture_orphans
from tools.npr_room.build_story_room import EXAMPLE
from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.mesh_export import export_mesh_geometry, read_unity_mesh
from tools.npr_room.pbr_materials import staging_metadata, write
from tools.npr_room.reference_inventory import asset_index, digest, group_paths_by_guid
from tools.npr_room.room_geometry import copy_room_asset
from tools.npr_room.room_surfaces import base_surface, mounted_assets, stage_textures
from tools.npr_room.skinned_mesh_export import UnityScene, append_skin
from tools.npr_room.story_chapters import lua_value
from tools.npr_room.window_scene_inventory import ROOM_SCENE


HEROINE_USE_OBJECT = "228b52e0b215c1c1c929445a6701377d"
STATES = {"Pc": "FromBookToPc", "Book": "FromPcToBook", "Report": "FromPcToReport"}
PATHS = ("m_keyboard", "m_reportnote", "m_textbook/root")
TEXTBOOK = "desk-textbook"
FACTORY = "local function new_room_opening(roomLanguage, level, setMotion)"
BODY = "local story_body_states = "


def converted_quaternion(value):
    q = [-value["x"], -value["y"], value["z"], value["w"]]
    length = math.hypot(*q)
    if not math.isfinite(length) or length == 0:
        raise ValueError("Invalid desk quaternion")
    return [component / length for component in q]


def euler_xyz(value):
    x, y, z, w = converted_quaternion(value)
    sine = 2 * (x * z + w * y)
    if abs(sine) >= 1 - 1e-12:
        raise ValueError("Desk pose requires a singular Euler axis")
    return [math.degrees(angle) for angle in (
        math.atan2(2 * (w * x - y * z), 1 - 2 * (x*x + y*y)),
        math.asin(sine),
        math.atan2(2 * (w * z - x * y), 1 - 2 * (y*y + z*z)),
    )]


def children(scene, transform, path):
    for name in path.split("/"):
        items = [pointer["fileID"] for pointer in scene.read(transform)["m_Children"]]
        transform, = (item for item in items if scene.read(scene.read(item)["m_GameObject"]["fileID"])["m_Name"] == name)
    return transform


def component(scene, transform, class_id):
    game_object = scene.read(scene.read(transform)["m_GameObject"]["fileID"])
    result, = (pointer["component"]["fileID"] for pointer in game_object["m_Component"]
               if scene.documents[pointer["component"]["fileID"]][0] == class_id)
    return result


def endpoint(clip):
    if any(clip[field] for field in ("m_ScaleCurves", "m_FloatCurves", "m_EulerCurves", "m_PPtrCurves")):
        raise ValueError("Desk animator contains more than rigid transforms")
    stop = clip["m_AnimationClipSettings"]["m_StopTime"]
    result = {}
    for category in ("Position", "Rotation"):
        curves = clip[f"m_{category}Curves"]
        if len(curves) != len(PATHS) or {curve["path"] for curve in curves} != set(PATHS):
            raise ValueError("Desk animator does not bind all three source objects")
        for curve in curves:
            key = curve["curve"]["m_Curve"][-1]
            if not math.isclose(key["time"], stop, abs_tol=1e-5):
                raise ValueError("Expected an explicit key at the desk endpoint")
            result.setdefault(curve["path"], {})[category] = key["value"]
    return stop, result


def load_source(source, scene):
    assets = group_paths_by_guid(asset_index(source))

    def resolve(pointer):
        relative, = assets[pointer["guid"]]
        path = source / relative
        return path, UnityScene(path).read(pointer["fileID"])

    actor, = (identity for identity, row in scene.documents.items() if row[0] == 114
              and scene.read(identity).get("m_Script", {}).get("guid") == HEROINE_USE_OBJECT)
    animator_id = scene.read(actor)["_deskObjectAnimator"]["fileID"]
    animator = scene.read(animator_id)
    workspace, = (pointer["component"]["fileID"] for pointer in scene.read(animator["m_GameObject"]["fileID"])["m_Component"]
                  if scene.documents[pointer["component"]["fileID"]][0] == 4)
    path, = assets[animator["m_Controller"]["guid"]]
    controller_path = source / path
    controller = UnityScene(controller_path)
    source_states = {record["m_Name"]: (identity, record) for identity, row in controller.documents.items()
                     if row[0] == 1102 and (record := controller.read(identity))["m_Name"] in STATES.values()}
    poses, receipts = {}, {}
    for name, source_state in STATES.items():
        identity, record = source_states[source_state]
        clip_path, clip = resolve(record["m_Motion"])
        time, pose = endpoint(clip)
        poses[name] = pose
        receipts[name] = {"state": source_state, "identity": identity, "sampleSecond": time,
                          "source": clip_path.relative_to(source).as_posix(), "sha256": digest(clip_path)}

    textbook = children(scene, workspace, "m_textbook")
    textbook_animator = scene.read(component(scene, textbook, 95))
    ctrl_path, = assets[textbook_animator["m_Controller"]["guid"]]
    ctrl = UnityScene(source / ctrl_path)
    ctrl_doc = ctrl.read(textbook_animator["m_Controller"]["fileID"])
    layer, = ctrl_doc["m_AnimatorLayers"]
    default = ctrl.read(layer["m_StateMachine"]["fileID"])["m_DefaultState"]["fileID"]
    idle_path, idle = resolve(ctrl.read(default)["m_Motion"])
    renderer = component(scene, children(scene, textbook, "m_textbook"), 137)
    return workspace, textbook, renderer, poses, receipts, idle, idle_path, controller_path, ctrl_path, assets


def stage_textbook(source, catalog, scene, textbook, renderer_id, idle, references, assets, level, parent):
    renderer = scene.read(renderer_id)
    nodes, bones, mesh_node = scene.skin_hierarchy(renderer)
    root = children(scene, textbook, "root")
    retained = set(bones) | {mesh_node, textbook, root}
    if len(bones) != 11 or renderer["m_BlendShapeWeights"] or not renderer["m_Enabled"]:
        raise ValueError("Expected the original textbook skin")
    nodes = {key: value for key, value in nodes.items() if key in retained}
    nodes[root].update(translation=[0, 0, 0], rotation=[0, 0, 0, 1], scale=[1, 1, 1])
    if any(idle[key] for key in ("m_PositionCurves", "m_EulerCurves", "m_ScaleCurves", "m_FloatCurves", "m_PPtrCurves")):
        raise ValueError("Expected a rotation-only resting textbook pose")
    curves = idle["m_RotationCurves"]
    if len(curves) != 9:
        raise ValueError("Expected the nine original resting page bindings")
    for row in curves:
        keys = row["curve"]["m_Curve"]
        if any(key["value"] != keys[0]["value"] for key in keys):
            raise ValueError("Textbook default has moving page curves")
        nodes[children(scene, textbook, row["path"])]["rotation"] = converted_quaternion(keys[0]["value"])
    mesh_path, = references[renderer["m_Mesh"]["guid"]]
    mesh = read_unity_mesh(source / mesh_path)
    model, blob = export_mesh_geometry(mesh)
    binary = bytearray(blob)
    append_skin(model, binary, mesh, nodes, bones, mesh_node)
    model["buffers"][0]["byteLength"] = len(binary)
    folder = assets / "desk"
    folder.mkdir()
    destination = folder / "textbook.glb"
    write_glb(destination, model, bytes(binary))
    material, = renderer["m_Materials"]
    guid = material["guid"]
    source_slot = json_file(catalog / "source-lock/material-catalog.json")["slots"][guid]
    if digest(source / source_slot["source"]) != source_slot["sourceSHA256"]:
        raise ValueError("Textbook material changed")
    image_root = catalog / "image-catalog"
    slots = json_file(image_root / "image-catalog.json")["slots"]
    surface, texture = base_surface(source_slot, slots)
    mounted = mounted_assets(assets)
    uid = str(UUID(hex=guid))
    if uid not in mounted:
        path = folder / "textbook.azurematerial"
        write(path, surface)
        staging_metadata(path, guid, [str(UUID(hex=texture))] if texture else [])
    elif json_file(mounted[uid]) != surface:
        raise ValueError("Mounted textbook material differs")
    images = stage_textures(assets, folder, image_root, slots, [texture] if texture else [], mounted)
    level["resources"].append({"id": TEXTBOOK, "asset": "assets:/desk/textbook.glb"})
    node = {"id": TEXTBOOK, "name": "m_textbook", "parentId": parent, "resourceId": TEXTBOOK,
            "materialSlots": {"name:unity-slot-0": uid},
            "components": {"azure.transform": {"type": "azure.transform", "version": 1,
                                               "data": {"scale": [1, 1, 1]}}}}
    level["nodes"].append(node)
    return {"renderer": renderer_id, "mesh": mesh_path, "sourceMeshSHA256": digest(source / mesh_path),
            "textbookGLBSHA256": digest(destination), "boneCount": len(bones), "pages": len(curves),
            "materialSHA256": source_slot["sourceSHA256"], "textures": images}


def apply_pose(level, poses):
    nodes = {node["id"]: node for node in level["nodes"]}
    for record in poses:
        data = nodes[record["node"]]["components"]["azure.transform"]["data"]
        data.update(translation=record["translation"], rotation=record["rotation"])


def integrate(script, poses):
    end_marker = "new_story_body(setMotion, story_body_states))"
    if any(script.count(marker) != 1 for marker in (START, BODY, FACTORY, end_marker)):
        raise ValueError("Expected the normal story and body factory")
    prefix = (EXAMPLE / "story-desk.lua").read_text(encoding="utf-8")
    story = (EXAMPLE / "story-room.lua").read_text(encoding="utf-8")
    config = "local story_desk_poses = " + lua_value(poses)
    return (script[:script.index(START)] + "\n".join((prefix, story, config, script[script.index(BODY):]))
            ).replace(end_marker, end_marker[:-1] + ", new_story_desk(self, story_desk_poses))")


def stage(source, catalog, room, output):
    scene_path = source / ROOM_SCENE
    scene_sha = digest(scene_path)
    if scene_sha != json_file(catalog / "source-lock/window-scenes.json")["roomSceneSHA256"]:
        raise ValueError("Desk layout requires the locked room scene")
    scene = UnityScene(scene_path)
    workspace, textbook, renderer, originals, receipts, idle, idle_path, controller, textbook_ctrl, references = load_source(source, scene)
    with tempfile.TemporaryDirectory(prefix="story-desk-", dir=output.parent) as directory:
        project = Path(directory) / "project"
        assets = project / "assets"
        shutil.copytree(room / "assets", assets, copy_function=copy_room_asset, ignore=skip_capture_orphans)
        for path in [room / "project.azureproject", *room.glob("*-receipt.json")]:
            shutil.copyfile(path, project / path.name)
        scene_path, program = assets / "room.azurelevel", assets / "pomodoro.lua"
        level = json_file(scene_path)
        mapping = level["sceneImports"]["RoomScene"]["nodeMap"]
        source_nodes = {path: mapping[f"unity-{children(scene, workspace, path)}"] for path in PATHS[:2]}
        source_nodes[PATHS[2]] = TEXTBOOK
        skin = stage_textbook(source, catalog, scene, textbook, renderer, idle, references, assets, level,
                              mapping[f"unity-{workspace}"])
        poses = {}
        for name, original in originals.items():
            poses[name] = [{"node": source_nodes[path], "translation": [
                value["Position"]["x"], value["Position"]["y"], -value["Position"]["z"]],
                "rotation": euler_xyz(value["Rotation"])} for path, value in original.items()]
        apply_pose(level, poses["Pc"])
        write(scene_path, level)
        code = integrate(program.read_text(encoding="utf-8"), poses)
        program.write_text(code, encoding="utf-8", newline="\n")
        story = json_file(project / "opening-story-room-receipt.json")
        story.update(levelSHA256=digest(scene_path), scriptSHA256=digest(program))
        write(project / "opening-story-room-receipt.json", story)
        receipt = {"schemaVersion": 1, "sceneSHA256": scene_sha, "deskControllerSHA256": digest(controller),
                   "textbookControllerSHA256": digest(source / textbook_ctrl), "textbookIdleSHA256": digest(idle_path),
                   "endpointClips": receipts, "poses": poses, "textbook": skin,
                   "levelSHA256": story["levelSHA256"], "scriptSHA256": story["scriptSHA256"]}
        write(project / "opening-desk-receipt.json", receipt)
        publish_directory(project, output)
    return receipt


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for arg in ("source", "catalog", "room", "output"):
        parser.add_argument("--" + arg, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(stage(args.source, args.catalog, args.room, args.output)))
