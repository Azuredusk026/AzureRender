"""Read the original room's glitch camera, target, and activation settings."""

import argparse
import json
from pathlib import Path

from tools.build_face_sdf import parse_glb
from tools.npr_room.reference_inventory import META_GUID, digest, text
from tools.npr_room.skinned_mesh_export import UnityScene
from tools.npr_room.window_scene_inventory import ROOM_SCENE


DIRECTION = "Assets/Scripts/Assembly-CSharp/GlitchDirection.cs"
TARGET = "Assets/RenderTexture/GlitchScreen.renderTexture"
LAYERS = "ProjectSettings/TagManager.asset"


def guid(path):
    return META_GUID.search(text(path.with_suffix(path.suffix + ".meta"))).group(1)


def camera_source(source, effects):
    locked = json.loads(effects.read_bytes())
    scene_path = source / ROOM_SCENE
    if digest(scene_path) != locked["sceneSHA256"]:
        raise ValueError("Glitch camera requires the source-locked room scene")
    scene = UnityScene(scene_path)
    controller_id = scene.read(locked["serviceId"])["_glitch"]["fileID"]
    controller = scene.read(controller_id)
    if controller["m_Script"]["guid"] != guid(source / DIRECTION):
        raise ValueError("Unexpected original glitch controller")
    glitch, = (row for row in locked["renderers"]["High"]["passes"] if row["shaderProgram"] == "glitch")
    material_file = source / glitch["material"]
    material_pointer = controller["_glitch1Material"]
    if guid(material_file) != material_pointer["guid"] or digest(material_file) != glitch["materialSHA256"]:
        raise ValueError("The glitch controller material differs from the renderer")
    material = UnityScene(material_file).read(material_pointer["fileID"])
    bindings = {name: value for slot in material["m_SavedProperties"]["m_TexEnvs"] for name, value in slot.items()}
    output_pointer = bindings["_RenderTexture"]["m_Texture"]
    target_path = source / TARGET
    if output_pointer["guid"] != guid(target_path):
        raise ValueError("Glitch material points to a different render target")

    object_id = controller["_glitchCameraObj"]["fileID"]
    obj = scene.read(object_id)
    components = [entry["component"]["fileID"] for entry in obj["m_Component"]]
    transform_id, = (identity for identity in components if scene.documents[identity][0] == 4)
    camera_id, = (identity for identity in components if scene.documents[identity][0] == 20)
    settings_id, = (identity for identity in components if scene.documents[identity][0] == 114)
    transform = scene.read(transform_id)
    camera = scene.read(camera_id)
    settings = scene.read(settings_id)
    parent_id = transform["m_Father"]["fileID"]
    parent_obj = scene.read(scene.read(parent_id)["m_GameObject"]["fileID"])
    if camera["m_TargetTexture"] != output_pointer or obj["m_IsActive"] or parent_obj["m_Name"] != "MainCamera":
        raise ValueError("Unexpected original glitch camera binding")
    target = UnityScene(target_path).read(output_pointer["fileID"])
    layer_path = source / LAYERS
    layers = UnityScene(layer_path).read(1)["layers"]
    mask = camera["m_CullingMask"]["m_Bits"]
    return {
        "schemaVersion": 1,
        "effectsSHA256": digest(effects),
        "sceneSHA256": locked["sceneSHA256"],
        "controller": {
            "fileId": controller_id,
            "scriptSHA256": digest(source / DIRECTION),
            "enabledIntervalSeconds": [controller["_cameraChangeActiveSecondsMin"],
                                       controller["_cameraChangeActiveSecondsMax"]],
        },
        "camera": {
            "fileId": camera_id, "objectId": object_id, "parentTransformId": parent_id,
            "initialActive": bool(obj["m_IsActive"]),
            "localPosition": transform["m_LocalPosition"], "localRotation": transform["m_LocalRotation"],
            "fieldOfView": camera["field of view"], "clips": [camera["near clip plane"], camera["far clip plane"]],
            "clearFlags": camera["m_ClearFlags"], "background": camera["m_BackGroundColor"],
            "cullingMask": mask, "layers": [name for bit, name in enumerate(layers) if mask & (1 << bit)],
            "layersSHA256": digest(layer_path),
            "renderer": settings["m_RendererIndex"], "renderShadows": bool(settings["m_RenderShadows"]),
            "postProcessing": bool(settings["m_RenderPostProcessing"]), "antialiasing": settings["m_Antialiasing"],
            "volumeMask": settings["m_VolumeLayerMask"]["m_Bits"],
        },
        "target": {
            "source": TARGET, "sha256": digest(target_path), "fileId": output_pointer["fileID"],
            "size": [target["m_Width"], target["m_Height"]], "srgb": bool(target["m_SRGB"]),
            "colorFormat": target["m_ColorFormat"], "depthStencilFormat": target["m_DepthStencilFormat"],
            "samples": target["m_AntiAliasing"], "mipmapped": bool(target["m_MipMap"]),
            "filter": target["m_TextureSettings"]["m_FilterMode"],
            "wrap": [target["m_TextureSettings"][f"m_Wrap{axis}"] for axis in "UVW"],
        },
    }


def layer_rows(bindings, read, mask):
    rows = []
    for node, (identity, kind) in sorted(bindings.items()):
        obj = read(read(identity)["m_GameObject"]["fileID"])
        layer = obj["m_Layer"]
        rows.append({"node": node, "sourceId": identity, "sourceKind": kind, "name": obj["m_Name"],
                     "layer": layer, "included": bool(mask & (1 << layer))})
    return rows


def room_layers(source, room, camera):
    scene_path = source / ROOM_SCENE
    if digest(scene_path) != camera["sceneSHA256"]:
        raise ValueError("The room drawables require the source-locked Unity scene")
    scene = UnityScene(scene_path)
    level_path = room / "assets/room.azurelevel"
    level = json.loads(level_path.read_bytes())
    original = {node: int(key.removeprefix("unity-").removeprefix("batch-"))
                for key, node in level["sceneImports"]["RoomScene"]["nodeMap"].items()
                if key.startswith("unity-")}
    resources = {entry["id"]: entry["asset"] for entry in level["resources"]}
    desk = json.loads((room / "opening-desk-receipt.json").read_bytes())
    if desk["sceneSHA256"] != camera["sceneSHA256"]:
        raise ValueError("The staged textbook has a different source scene")
    skin_renderers = {}
    for identity, (kind, *_) in scene.documents.items():
        if kind == 137:
            name = scene.read(scene.read(identity)["m_GameObject"]["fileID"])["m_Name"]
            skin_renderers.setdefault(name, []).append(identity)
    bindings = {}
    for node in level["nodes"]:
        if not node.get("resourceId"):
            continue
        name = node["id"]
        if name in original:
            bindings[name] = (original[name], "transform")
        elif name.startswith(("story-books-", "window-city-")):
            bindings[name] = (int(name.rsplit("-", 1)[-1]), "transform")
        elif name == "desk-textbook":
            bindings[name] = (desk["textbook"]["renderer"], "renderer")
        elif name.startswith("heroine-"):
            relative = resources[node["resourceId"]]
            if not relative.startswith("assets:/heroine/"):
                raise ValueError(f"The heroine drawable has a different asset binding: {name}")
            model, _ = parse_glb(room / "assets" / relative.removeprefix("assets:/"))
            mesh, = model["meshes"]
            identity, = skin_renderers[mesh["name"]]
            bindings[name] = (identity, "renderer")
        else:
            raise ValueError(f"No source layer for staged room drawable: {name}")
    return {"schemaVersion": 1, "sceneSHA256": camera["sceneSHA256"], "levelSHA256": digest(level_path),
            "cullingMask": camera["camera"]["cullingMask"],
            "drawables": layer_rows(bindings, scene.read, camera["camera"]["cullingMask"])}


def write_receipt(path, receipt):
    with path.open("x", encoding="utf-8", newline="\n") as output:
        output.write(json.dumps(receipt, sort_keys=True, separators=(",", ":")) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source", "effects", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--room", type=Path)
    parser.add_argument("--layer-output", type=Path)
    args = parser.parse_args()
    if bool(args.room) != bool(args.layer_output):
        parser.error("--room and --layer-output must be supplied together")
    receipt = camera_source(args.source, args.effects)
    if args.room:
        write_receipt(args.layer_output, room_layers(args.source, args.room, receipt))
    write_receipt(args.output, receipt)
    print(json.dumps(receipt))
