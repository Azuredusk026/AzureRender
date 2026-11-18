"""Convert the original texture-free transparent material blend and depth state."""

import math
import json
from uuid import UUID

from tools.npr_room.pbr_materials import staging_metadata
from tools.npr_room.reference_inventory import digest, META_GUID
from tools.npr_room.skinned_mesh_export import UnityScene


TRANSPARENT_SHADER = "28226603f4d8e8342957ab997e1e3b94"
LENS_GUID = "15f3f21152ae6e4448d051e79053f0f5"


def transparent_document(source):
    properties = source["m_SavedProperties"]
    floats = {name: value for row in properties["m_Floats"] for name, value in row.items()}
    colors = {name: value for row in properties["m_Colors"] for name, value in row.items()}
    textures = {name: value for row in properties["m_TexEnvs"] for name, value in row.items()
                if value["m_Texture"]["fileID"]}
    features = ("_UseMain2ndTex", "_UseMain3rdTex", "_UseEmission", "_UseEmission2nd",
                "_UseMatCap", "_UseMatCap2nd", "_UseRim", "_AlphaMaskMode", "_AsUnlit")
    if (source["m_Shader"].get("guid") != TRANSPARENT_SHADER or source["m_Parent"]["fileID"]
            or floats["_SrcBlend"] != 1 or floats["_DstBlend"] != 10 or floats["_ZTest"] != 4
            or floats["_ZWrite"] not in (0, 1) or floats["_Cull"] not in (0, 2)
            or textures or any(floats.get(name, 0) for name in features)):
        raise ValueError("Transparent source requires texture-free premultiplied blending and supported depth")
    rgba = [colors["_Color"][channel] for channel in "rgba"]
    roughness, metallic = 1 - floats["_Smoothness"], floats["_Metallic"]
    if not all(math.isfinite(value) and 0 <= value <= 1 for value in (*rgba, roughness, metallic)):
        raise ValueError("Transparent source factors must be finite normalized values")
    material = {"name": "original-transparent", "alphaMode": "BLEND", "doubleSided": floats["_Cull"] == 0,
                "pbrMetallicRoughness": {"baseColorFactor": rgba, "roughnessFactor": roughness,
                                         "metallicFactor": metallic},
                "extras": {"azureRenderMaterial": {"schemaVersion": 2, "class": "generic", "features": [],
                    "blendDepth": {"schemaVersion": 1, "write": bool(floats["_ZWrite"]), "compare": "less-or-equal"}}}}
    return {"schemaVersion": 3, "typeId": "azure.profile", "parameters": {}, "textures": {},
            "surface": {"asset": {"version": "2.0"}, "materials": [material]}}


def install_lens(assets, level, source):
    material_path = source / "Assets/Material/mt_Lens.mat"
    guid, = META_GUID.findall(material_path.with_suffix(".mat.meta").read_text(encoding="utf-8"))
    if guid != LENS_GUID:
        raise ValueError("The original lens material identity differs")
    scene = UnityScene(material_path)
    material_id, = [identity for identity, document in scene.documents.items() if document[0] == 21]
    material = scene.read(material_id)
    document = transparent_document(material)
    identity = level["sceneImports"]["RoomScene"]["nodeMap"]["unity-8186"]
    node, = [node for node in level["nodes"] if node["id"] == identity]
    asset_id = str(UUID(hex=guid))
    slot = "name:unity-slot-0"
    existing = node.get("materialSlots", {}).get(slot)
    if existing is not None and existing != asset_id:
        raise ValueError("The original lens slot already has a different material")
    target = assets / "original-transparent/lens.azurematerial"
    encoded = json.dumps(document, sort_keys=True, separators=(",", ":")) + "\n"
    if target.exists() and target.read_text(encoding="utf-8") != encoded:
        raise ValueError("The staged lens material differs from its original source")
    metadata = target.with_name(target.name + ".azmeta")
    if metadata.exists() and json.loads(metadata.read_bytes())["id"] != asset_id:
        raise ValueError("The staged lens material metadata has another identity")
    target.parent.mkdir(exist_ok=True)
    if not target.exists():
        target.write_text(encoded, encoding="utf-8", newline="\n")
    if not metadata.exists():
        staging_metadata(target, guid, [])
    node.setdefault("materialSlots", {})[slot] = asset_id
    return {"sourceSHA256": digest(material_path), "materialSHA256": digest(target),
            "sourceGuid": guid, "node": identity, "slot": slot,
            "alphaMode": "BLEND", "alpha": document["surface"]["materials"][0]["pbrMetallicRoughness"]["baseColorFactor"][3]}
