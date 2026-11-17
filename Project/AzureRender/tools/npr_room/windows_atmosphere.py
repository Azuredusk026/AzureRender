"""Bind the Windows atmosphere inputs used by the original room service."""

import argparse
import json
import math
from pathlib import Path

from tools.npr_room.build_city_window import verified_path
from tools.npr_room.reference_inventory import META_GUID, digest
from tools.npr_room.skinned_mesh_export import UnityScene
from tools.npr_room.unity_d3d11_programs import ATMOSPHERE
from tools.npr_room.window_scene_inventory import ROOM_SCENE, SOURCE_CODE


MODES = ("Day", "Sunset", "Night", "Cloudy")
SERVICE = SOURCE_CODE + "/MyCozyWeatherService.cs"
TEXTURES = {"cloudTexture", "chemtrailsTexture", "cirrusCloudTexture", "altocumulusCloudTexture",
            "cirrostratusCloudTexture", "starMap", "galaxyMap", "galaxyStarMap", "galaxyVariationMap",
            "lightScatteringMap"}


def matches(scene, kind, value):
    for identity, (declared, start, end) in scene.documents.items():
        if declared == kind and scene.serialized.find(value, start, end) != -1:
            yield identity, scene.read(identity)


def renderer(scene, identity):
    entry = scene.read(identity)
    if scene.documents[identity][0] != 23:
        raise ValueError(f"Expected a mesh renderer for the atmosphere: {identity}")
    entity = entry["m_GameObject"]["fileID"]
    game = scene.read(entity)
    components = {scene.documents[row["component"]["fileID"]][0]: row["component"]["fileID"]
                  for row in game["m_Component"]}
    mesh = scene.read(components[33])["m_Mesh"]
    return {"name": game["m_Name"], "renderer": identity, "transform": components[4],
            "gameObject": entity, "mesh": mesh, "materials": entry["m_Materials"]}


def bind(source, catalog, captures, d3d_programs):
    scene_path = source / ROOM_SCENE
    source_code = source / SERVICE
    guid = META_GUID.search(source_code.with_suffix(".cs.meta").read_text(encoding="utf-8")).group(1)
    scene = UnityScene(scene_path)
    (service_id, service), = [(identity, row) for identity, row in matches(scene, 114, guid)
                              if row["m_Script"].get("guid") == guid]
    base_id, base = next((identity, row) for identity, row in matches(scene, 1, "BaseCozy Weather Sphere")
                         if row["m_Name"] == "BaseCozy Weather Sphere")
    (base_weather_id, base_weather), = [(record["component"]["fileID"], scene.read(record["component"]["fileID"]))
                                       for record in base["m_Component"] if scene.documents[record["component"]["fileID"]][0] == 114]
    programs = json.loads(d3d_programs.read_bytes())["programs"]
    material_slots = json.loads((catalog / "source-lock/material-catalog.json").read_bytes())["slots"]
    drawables = {}
    for field, role in (("skyMesh", "sky"), ("cloudMesh", "cozy-cloud"), ("fogMesh", "fog")):
        item = renderer(scene, base_weather[field]["fileID"])
        material, = item["materials"]
        slot = material_slots[material["guid"]]
        shader = programs[role]["shader"]
        unity_export = "Assets/Shader/" + shader.replace("/", "_") + ".shader"
        if (shader != ATMOSPHERE[role] or slot["shader"]["source"] != unity_export
                or digest(source / slot["source"]) != slot["sourceSHA256"]):
            raise ValueError(f"Original Windows shader does not match its serialized material: {field}")
        drawables[field] = {**item, "shaderProgram": role, "materialSHA256": slot["sourceSHA256"]}
    asset_root = catalog / "image-catalog"
    assets = json.loads((asset_root / "image-catalog.json").read_bytes())["slots"]
    textures = {}
    for field in sorted(TEXTURES):
        reference = base_weather[field]
        if not reference["fileID"]:
            continue
        asset = assets[reference["guid"]]
        verified_path(asset_root, asset)
        if digest(source / asset["source"]) != asset["sha256"]:
            raise ValueError(f"Original atmosphere image bytes differ: {field}")
        textures[field] = {"guid": reference["guid"], **{key: asset[key] for key in ("source", "media", "sha256", "srgb")}}
    authored, effective = {}, {}
    for index, mode in enumerate(MODES, 11):
        key = mode.lower()
        file_id = service[f"_{key}CozyWeather"]["fileID"]
        sky = scene.read(file_id)
        time_id = sky["timeModule"]["fileID"]
        time = scene.read(time_id)
        if time["cachedWeatherSphere"]["fileID"] != file_id or not 0 <= time["m_DayPercentage"] < 1:
            raise ValueError(f"Invalid assigned atmosphere clock for {mode}")
        weather = next(row["WeatherProf"] for row in service["_weatherProfiles"] if row["WeatherType"] == index - 10)
        atmosphere = next(row["AtomosphereProf"] for row in service["_atmosphereProfiles"]
                          if row["WeatherType"] == index - 10)
        authored[mode] = {"window": index, "cozyWeather": file_id, "timeModule": time_id,
                          "dayFraction": time["m_DayPercentage"], "sunDayFraction": time["modifiedDayPercentage"],
                          "profile": weather, "atmosphereProfile": atmosphere,
                          "mobileSkyGameObject": service[f"_mobile{mode}Sky"]["fileID"]}
        snapshot = captures / f"{index}-{mode}-cozy.json"
        values = json.loads(snapshot.read_bytes())
        moon = values["moonDirection"]
        if not math.isclose(math.hypot(*(moon[axis] for axis in "xyz")), 1, abs_tol=1e-5):
            raise ValueError(f"Original Windows atmosphere snapshot has an invalid moon direction: {mode}")
        effective[mode] = {"file": snapshot.name, "sha256": digest(snapshot), "data": values}
    return {"schemaVersion": 1, "sceneSHA256": digest(scene_path), "serviceSHA256": digest(source_code),
            "d3d11ProgramSHA256": digest(d3d_programs), "sourceService": service_id,
            "base": {"gameObject": base_id, "weatherComponent": base_weather_id,
                     "drawables": drawables, "textures": textures, "customPivot": service["_cozyWeatherEffectPosition"]},
            "modes": authored, "windowsReference": effective}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("source", "catalog", "captures", "d3d-programs", "output"):
        parser.add_argument("--" + option, required=True, type=Path)
    args = parser.parse_args()
    data = bind(args.source, args.catalog, args.captures, args.d3d_programs)
    with args.output.open("x", encoding="utf-8", newline="\n") as output:
        json.dump(data, output, sort_keys=True, separators=(",", ":"))
        output.write("\n")
    print(json.dumps({"modes": list(data["modes"]), "meshRenderers": len(data["base"]["drawables"]),
                      "textureSources": len(data["base"]["textures"]), "sha256": digest(args.output)}))
