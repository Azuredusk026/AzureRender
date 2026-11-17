"""Lock the original Windows room effects from renderer to shader and texture."""

import argparse
from hashlib import sha256
import json
from pathlib import Path

from tools.npr_room.build_city_window import verified_path
from tools.npr_room.reference_inventory import META_GUID, asset_index, digest, group_paths_by_guid, text
from tools.npr_room.skinned_mesh_export import UnityScene
from tools.npr_room.unity_d3d11_programs import OPENING_EFFECTS
from tools.npr_room.window_scene_inventory import ROOM_SCENE
from tools.npr_room.windows_atmosphere import matches


SERVICE = "Assets/Scripts/Assembly-CSharp/DirectionService.cs"
FEATURES = {"ColorGlitch": "chromatic", "ScreenMove": "screen-move", "Glitch": "glitch",
            "Blackout": "blackout", "SimpleNoise": "noise"}
RUNTIME_TEXTURES = {"_BlitTexture", "_CameraOpaqueTexture", "_RenderTexture"}


def native_states(path, programs):
    import UnityPy

    selected = {row["sourcePathId"]: (role, row) for role, row in programs.items()}
    states = {}
    for shader in UnityPy.load(str(path)).objects:
        if shader.type.name != "Shader" or shader.path_id not in selected:
            continue
        role, source = selected[shader.path_id]
        if sha256(shader.get_raw_data()).hexdigest() != source["sourceObjectSHA256"]:
            raise ValueError(f"Original effect program changed: {role}")
        native = shader.read_typetree()["m_ParsedForm"]["m_SubShaders"][0]["m_Passes"][source["pass"]]["m_State"]
        states[role] = {"passName": native["m_Name"],
                        "blend": {name: value["val"] for name, value in native["rtBlend0"].items()},
                        **{name: native[name]["val"] for name in ("zTest", "zWrite", "culling")}}
    if states.keys() != programs.keys():
        raise ValueError("Original effect archive is incomplete")
    return states


def bind(source, catalog, native_path, program_path):
    locked = json.loads(program_path.read_bytes())
    programs = locked["programs"]
    if digest(native_path) != locked["sourceSHA256"] or programs.keys() != OPENING_EFFECTS.keys():
        raise ValueError("Opening effects require the pinned original Windows shader archive")
    render_states = native_states(native_path, programs)
    indexed = group_paths_by_guid(asset_index(source))
    material_slots = json.loads((catalog / "source-lock/material-catalog.json").read_bytes())["slots"]
    image_root = catalog / "image-catalog"
    image_slots = json.loads((image_root / "image-catalog.json").read_bytes())["slots"]
    source_path, scene_path = source / SERVICE, source / ROOM_SCENE
    guid = META_GUID.search(text(source_path.with_suffix(".cs.meta"))).group(1)
    (service_id, service), = [(identity, row) for identity, row in matches(UnityScene(scene_path), 114, guid)
                              if row["m_Script"].get("guid") == guid]

    def document(pointer):
        relative, = indexed[pointer["guid"]]
        path = source / relative
        return relative, digest(path), UnityScene(path).read(pointer["fileID"])

    def feature(role, location, checksum, row):
        slot = material_slots[row["passMaterial"]["guid"]]
        program = programs[role]
        if (row["passIndex"] != program["pass"] or row["injectionPoint"] != 600 or not row["fetchColorBuffer"]
                or program["shader"] != OPENING_EFFECTS[role]
                or slot["shader"]["source"] != f"Assets/Shader/{program['shader'].replace('/', '_')}.shader"
                or digest(source / slot["source"]) != slot["sourceSHA256"]):
            raise ValueError(f"Original effect material differs: {location}")
        material = UnityScene(source / slot["source"]).read(row["passMaterial"]["fileID"])
        properties = material["m_SavedProperties"]
        references = {key: record for item in properties["m_TexEnvs"] for key, record in item.items()}
        textures = {}
        for record in program["stages"]["fragment"]["textures"]:
            name = record["name"]
            if name in RUNTIME_TEXTURES:
                textures[name] = {"runtime": True, "binding": record["binding"]}
                continue
            reference = references[name]
            asset = image_slots[reference["m_Texture"]["guid"]]
            verified_path(image_root, asset)
            if digest(source / asset["source"]) != asset["sha256"]:
                raise ValueError(f"Original effect texture differs: {asset['source']}")
            textures[name] = {**asset, "binding": record["binding"], "uvScale": reference["m_Scale"],
                              "uvOffset": reference["m_Offset"]}
        return {"feature": location, "featureSHA256": checksum, "injectionPoint": row["injectionPoint"],
                "fetchColor": bool(row["fetchColorBuffer"]), "material": slot["source"],
                "materialSHA256": slot["sourceSHA256"], "shaderProgram": role,
                "renderState": render_states[role], "textures": textures,
                "floats": {key: value for item in properties["m_Floats"] for key, value in item.items()}}

    renderers = {}
    for tier in ("Low", "Middle", "High"):
        location, checksum, renderer = document(service[f"_universalRendererData{tier}"])
        passes = []
        for pointer in renderer["m_RendererFeatures"]:
            path, digest_value, entry = document(pointer)
            if (role := FEATURES.get(entry["m_Name"])) is not None:
                passes.append(feature(role, path, digest_value, entry))
        if set(item["shaderProgram"] for item in passes) != programs.keys() or len(passes) != len(programs):
            raise ValueError(f"Original Windows renderer lacks a required opening effect: {location}")
        renderers[tier] = {"source": location, "sha256": checksum, "passes": passes}
    return {"schemaVersion": 1, "sceneSHA256": digest(scene_path), "serviceSHA256": digest(source_path),
            "programsSHA256": digest(program_path), "serviceId": service_id, "renderers": renderers}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("source", "catalog", "native", "programs", "output"):
        parser.add_argument("--" + option, type=Path, required=True)
    args = parser.parse_args()
    value = bind(args.source, args.catalog, args.native, args.programs)
    with args.output.open("x", encoding="utf-8", newline="\n") as output:
        json.dump(value, output, sort_keys=True, separators=(",", ":"))
        output.write("\n")
    print(json.dumps({"tiers": len(value["renderers"]), "sha256": digest(args.output)}))
