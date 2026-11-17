"""Read the original room's glitch camera, target, and activation settings."""

import argparse
import json
from pathlib import Path

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


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source", "effects", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    receipt = camera_source(args.source, args.effects)
    with args.output.open("x", encoding="utf-8", newline="\n") as output:
        output.write(json.dumps(receipt, sort_keys=True, separators=(",", ":")) + "\n")
    print(json.dumps(receipt))
