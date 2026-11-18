"""Prepare controlled original Direct3D cloud and fog draws for the Windows room."""

import argparse
from hashlib import sha256
import json
import math
from pathlib import Path
import zlib

import numpy as np
from PIL import Image

from tools.npr_room.build_city_window import verified_path
from tools.npr_room.mesh_export import attributes, indices, read_unity_mesh
from tools.npr_room.reference_inventory import digest
from tools.npr_room.skinned_mesh_export import UnityScene
from tools.npr_room.window_scene_inventory import ROOM_SCENE
from tools.npr_room.windows_sky_probe import (
    COZY_RUNTIME_SHA256, FACES, MODES, replay_globals, stage_constants, unity_rotation,
)


FACE_SIZE = 128
NEAR, FAR = .05, 1500
FOG_DEPTHS = (1., 3., 12., 30., 80., 150.)
CLOUD_TEXTURES = {
    "CZY_ChemtrailsTexture": "chemtrailsTexture", "CZY_CirrusTexture": "cirrusCloudTexture",
    "CZY_AltocumulusTexture": "altocumulusCloudTexture", "CZY_CirrostratusTexture": "cirrostratusCloudTexture",
}
COMMON_COLORS = {"CZY_FilterColor": "filterColor", "CZY_SunFilterColor": "sunFilter"}
COMMON_SCALARS = {"CZY_FilterSaturation": "filterSaturation", "CZY_FilterValue": "filterValue"}
CLOUD_COLORS = {
    "CZY_CloudColor": "cloudColor", "CZY_CloudFilterColor": "cloudFilter",
    "CZY_CloudHighlightColor": "cloudHighlightColor", "CZY_CloudMoonColor": "cloudMoonColor",
    "CZY_AltoCloudColor": "highAltitudeCloudColor",
}
CLOUD_SCALARS = {
    "CZY_WindSpeed": "cloudWindSpeed", "CZY_MainCloudScale": "cloudMainScale",
    "CZY_CumulusCoverageMultiplier": "cumulus", "CZY_SunFlareFalloff": "cloudSunHighlightFalloff",
    "CZY_CloudMoonFalloff": "cloudMoonHighlightFalloff", "CZY_DetailScale": "cloudDetailScale",
    "CZY_DetailAmount": "cloudDetailAmount", "CZY_BorderHeight": "borderHeight",
    "CZY_BorderVariation": "borderVariation", "CZY_BorderEffect": "borderEffect",
    "CZY_NimbusHeight": "nimbusHeightEffect", "CZY_NimbusMultiplier": "nimbus",
    "CZY_NimbusVariation": "nimbusVariation", "CZY_ChemtrailsMoveSpeed": "chemtrailsMoveSpeed",
    "CZY_ChemtrailsMultiplier": "chemtrails", "CZY_CirrusMoveSpeed": "cirrusMoveSpeed",
    "CZY_CirrusMultiplier": "cirrus", "CZY_ClippingThreshold": "clippingThreshold",
    "CZY_AltocumulusScale": "acScale", "CZY_AltocumulusMultiplier": "altocumulus",
    "CZY_CirrostratusMoveSpeed": "cirroMoveSpeed", "CZY_CirrostratusMultiplier": "cirrostratus",
}
FOG_COLORS = {
    "CZY_LightColor": "fogFlareColor", "CZY_FogMoonFlareColor": "fogMoonFlareColor",
    **{f"CZY_FogColor{i}": f"fogColor{i}" for i in range(1, 6)},
}
FOG_SCALARS = {
    "CZY_VariationScale": "fogVariationScale", "CZY_VariationAmount": "fogVariationAmount",
    "CZY_VariationDistance": "fogVariationDistance", "CZY_LightFlareSquish": "fogLightFlareSquish",
    "CZY_LightIntensity": "fogLightFlareIntensity", "CZY_LightFalloff": "fogLightFlareFalloff",
    "CZY_FogSmoothness": "fogSmoothness", "CZY_FogOffset": "fogHeight", "CZY_FogIntensity": "fogDensity",
    **{f"CZY_FogColorStart{i}": f"fogStart{i}" for i in range(1, 5)},
}


def depth_stripes(size):
    distances = np.array(FOG_DEPTHS)[np.arange(size) * len(FOG_DEPTHS) // size]
    reversed_z = NEAR * (FAR / distances - 1) / (FAR - NEAR)
    return np.tile(reversed_z, (size, 1)).astype("<f4")


def validate_clip_planes(near, far):
    if not math.isfinite(near) or not math.isfinite(far) or not 0 < near < far:
        raise ValueError("Weather camera requires finite ordered clip planes")


def fog_camera(forward, up, *, position=(0, 0, 0), vertical_fov=math.pi / 2, aspect=1,
               near=NEAR, far=FAR):
    validate_clip_planes(near, far)
    orientation = np.eye(4)
    orientation[:3, :3] = np.stack((np.cross(up, forward), up, forward), axis=1)
    if not np.allclose(orientation[:3, :3].T @ orientation[:3, :3], np.eye(3), atol=1e-6):
        raise ValueError("Weather camera requires an orthonormal view basis")
    orientation[:3, 3] = position
    focal = 1 / math.tan(vertical_fov / 2)
    lens = np.array([
        [focal / aspect, 0, 0, 0], [0, focal, 0, 0],
        [0, 0, far / (far - near), -near * far / (far - near)], [0, 0, 1, 0],
    ])
    # The original fragment converts reversed depth to OpenGL clip coordinates,
    # unprojects it in right-handed camera space, then flips the view-space Z.
    inverse_lens = lens.copy()
    inverse_lens[2, :] = 2 * lens[2, :] - lens[3, :]
    inverse_lens[:, 2] *= -1
    return (lens @ np.linalg.inv(orientation)).T.ravel(), {
        "unity_CameraInvProjection": np.linalg.inv(inverse_lens).T.ravel(),
        "unity_CameraToWorld": orientation.T.ravel(),
    }


def weather_fields(snapshot, mode, seconds, role, wind):
    _, base = replay_globals(snapshot, mode["sunDayFraction"], seconds)
    cloud = role == "cozy-cloud"
    colors = COMMON_COLORS | (CLOUD_COLORS if cloud else FOG_COLORS)
    scalars = COMMON_SCALARS | (CLOUD_SCALARS if cloud else FOG_SCALARS)
    fields = {name: tuple(snapshot[key][channel] for channel in "rgba") for name, key in colors.items()}
    fields.update({name: (snapshot[key],) for name, key in scalars.items()})
    fields.update({
        "_TimeParameters": (seconds, math.sin(seconds), math.cos(seconds), 0),
        "_WorldSpaceCameraPos": (0, 0, 0), "_UnderwaterRenderingEnabled": (0,), "_FullySubmerged": (0,),
        "CZY_SunDirection": base["CZY_SunDirection"][:3],
        "CZY_MoonDirection": tuple(snapshot["moonDirection"][axis] for axis in "xyz"),
    })
    if cloud:
        fields.update(CZY_StormDirection=wind, CZY_AltocumulusWindSpeed=(0, 0))
    else:
        fields.update(
            _GlobalMipBias=(0, 0),
            CZY_FogDepthMultiplier=(snapshot["fogDensityMultiplier"] * snapshot["fogDensity"],),
            CZY_VariationWindDirection=tuple(snapshot["fogVariationDirection"][axis] for axis in "xyz"),
        )
    return fields


def source_geometry(source, catalog, draw):
    entry = json.loads((catalog / "static-mesh-catalog/mesh-catalog.json").read_bytes())["slots"][draw["mesh"]["guid"]]
    mesh_path = source / entry["source"]
    if digest(mesh_path) != entry["sourceSHA256"]:
        raise ValueError("The original weather mesh differs")
    mesh = read_unity_mesh(mesh_path)
    rows = {key: np.array(value) for key, _, value in attributes(mesh)}
    position, normal = rows["POSITION"], rows["NORMAL"]
    uv = rows["TEXCOORD_0"]
    position[:, 2] *= -1
    normal[:, 2] *= -1
    uv[:, 1] = 1 - uv[:, 1]
    vertices = np.concatenate((position, normal, uv), axis=1).astype("<f4")
    triangles, = indices(mesh, len(vertices), mesh["m_SubMeshes"])
    triangles = np.asarray(triangles, dtype="<u4").reshape(-1, 3)[:, [0, 2, 1]]
    return vertices, triangles, entry["sourceSHA256"]


def payload(output, slot, width, height, srgb, data):
    (output / f"texture-{slot}.txt").write_text(f"{width} {height} {int(srgb)}\n", encoding="ascii")
    target = output / f"texture-{slot}.rgba.zlib"
    target.write_bytes(zlib.compress(data))
    return {"width": width, "height": height, "srgb": srgb, "rawSHA256": sha256(data).hexdigest()}


def prepare(source, catalog, references, programs, cozy_assembly, output, mode, role, seconds, wind):
    recipe = json.loads(references.read_bytes())
    program_path = programs / "windows-atmosphere-programs.json"
    scene_path = source / ROOM_SCENE
    if (digest(program_path) != recipe["d3d11ProgramSHA256"] or digest(scene_path) != recipe["sceneSHA256"]
            or digest(cozy_assembly) != COZY_RUNTIME_SHA256):
        raise ValueError("The weather probe requires the frozen scene, programs and Cozy runtime")
    stages = json.loads(program_path.read_bytes())["programs"][role]["stages"]
    cloud = role == "cozy-cloud"
    draw = recipe["base"]["drawables"]["cloudMesh" if cloud else "fogMesh"]
    scene = UnityScene(scene_path)
    transform = scene.read(draw["transform"])
    scale = tuple(transform["m_LocalScale"][axis] for axis in "xyz")
    model = unity_rotation(scene, draw) @ np.diag((*scale, 1))
    reference = recipe["windowsReference"][mode]
    fields = weather_fields(reference["data"], recipe["modes"][mode], seconds, role, wind)
    image_catalog = catalog / "image-catalog"

    material_guid = draw["materials"][0]["guid"]
    material_entry = json.loads((catalog / "source-lock/material-catalog.json").read_bytes())["slots"][material_guid]
    material_source = source / material_entry["source"]
    if digest(material_source) != draw["materialSHA256"]:
        raise ValueError("The weather material differs")
    fog_variation = None
    if not cloud:
        material = UnityScene(material_source).read(draw["materials"][0]["fileID"])
        binding, = (row["_FogVariationTexture"] for row in material["m_SavedProperties"]["m_TexEnvs"]
                    if "_FogVariationTexture" in row)
        image_entry = binding["m_Texture"]["guid"]
        fog_variation = json.loads((image_catalog / "image-catalog.json").read_bytes())["slots"][image_entry]

    vertices, triangles, mesh_digest = source_geometry(source, catalog, draw)
    output.mkdir()
    vertices.tofile(output / "vertices.bin")
    triangles.tofile(output / "indices.bin")
    (output / "object.bin").write_bytes(stage_constants(
        stages["vertex"], "UnityPerDraw", {"unity_ObjectToWorld": model.T.ravel()},
    ))
    for stage in stages.values():
        path = programs / stage["path"]
        if digest(path) != stage["sha256"]:
            raise ValueError("The original weather program differs")
        (output / path.name).write_bytes(path.read_bytes())
    if cloud:
        (output / "world.bin").write_bytes(stage_constants(stages["fragment"], "$Globals", fields))
    for index, (forward, up) in enumerate(FACES):
        view, matrices = fog_camera(forward, up)
        per_view = {"_ProjectionParams": (1, NEAR, FAR, 1 / FAR), "unity_MatrixVP": view}
        (output / f"view-{index}.bin").write_bytes(stage_constants(stages["vertex"], "$Globals", per_view))
        if not cloud:
            (output / f"world-{index}.bin").write_bytes(stage_constants(stages["fragment"], "$Globals", fields | matrices))

    textures = {}
    for row in stages["fragment"]["textures"]:
        binding, slot = row["name"], row["binding"]
        if binding == "_UnderwaterMask":
            info = payload(output, slot, 1, 1, False, bytes(4))
        elif binding == "_CameraDepthTexture":
            info = payload(output, slot, FACE_SIZE, FACE_SIZE, False, depth_stripes(FACE_SIZE).tobytes())
            info.update(format="R32_FLOAT", metres=FOG_DEPTHS)
        else:
            entry = fog_variation if binding == "_FogVariationTexture" else recipe["base"]["textures"][CLOUD_TEXTURES[binding]]
            path = verified_path(image_catalog, entry)
            with Image.open(path) as texture:
                info = payload(output, slot, texture.width, texture.height, entry["srgb"], texture.convert("RGBA").tobytes())
            info["sourceSHA256"] = entry["sha256"]
        textures[binding] = info

    return {"mode": mode, "program": role, "seconds": seconds, "resolution": [FACE_SIZE, FACE_SIZE],
            "referenceSHA256": reference["sha256"], "materialSHA256": draw["materialSHA256"],
            "programSHA256": {name: stage["sha256"] for name, stage in stages.items()},
            "cozyAssemblySHA256": COZY_RUNTIME_SHA256, "meshSHA256": mesh_digest,
            "vertexCount": len(vertices), "triangleCount": len(triangles), "meshScale": scale,
            "textures": textures, **({"controlledWindDirection": wind} if cloud else {})}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("source", "catalog", "references", "programs", "cozy-assembly", "output"):
        parser.add_argument("--" + option, type=Path, required=True)
    parser.add_argument("--mode", choices=MODES, required=True)
    parser.add_argument("--role", choices=("cozy-cloud", "fog"), required=True)
    parser.add_argument("--time", type=float, required=True)
    parser.add_argument("--wind", type=float, nargs=3)
    args = parser.parse_args()
    if args.role == "cozy-cloud":
        if args.wind is None or not math.isclose(math.hypot(*args.wind), 1, abs_tol=1e-6):
            parser.error("Cloud replay requires a fixed unit wind direction")
    elif args.wind is not None:
        parser.error("Fog uses its original variation wind field")
    result = prepare(args.source, args.catalog, args.references, args.programs, args.cozy_assembly, args.output,
                     args.mode, args.role, args.time, args.wind)
    (args.output / "preparation.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
