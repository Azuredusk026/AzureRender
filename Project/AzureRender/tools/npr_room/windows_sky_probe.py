"""Prepare a matching original Windows sky capture for an offscreen Direct3D draw."""

import argparse
from hashlib import sha256
import json
import math
from pathlib import Path
import re
from struct import pack_into
import zlib

import numpy as np
from PIL import Image

from tools.npr_room.build_city_window import verified_path
from tools.npr_room.mesh_export import attributes, indices, read_unity_mesh
from tools.npr_room.reference_inventory import digest
from tools.npr_room.skinned_mesh_export import UnityScene
from tools.npr_room.windows_atmosphere import MODES
from tools.npr_room.window_scene_inventory import ROOM_SCENE


SKY_TEXTURES = {"CZY_GalaxyVariationMap": "galaxyVariationMap", "CZY_StarMap": "starMap",
                "CZY_GalaxyMap": "galaxyMap", "CZY_GalaxyStarMap": "galaxyStarMap",
                "CZY_LightScatteringMap": "lightScatteringMap"}
SCALAR_FIELDS = {"CZY_FilterSaturation": "filterSaturation", "CZY_FilterValue": "filterValue",
                 "CZY_Power": "gradientExponent", "CZY_SunHaloFalloff": "sunFalloff", "CZY_SunSize": "sunSize",
                 "CZY_MoonFlareFalloff": "moonFalloff", "CZY_GalaxyMultiplier": "galaxyIntensity",
                 "CZY_RainbowSize": "rainbowPosition", "CZY_RainbowWidth": "rainbowWidth",
                 "CZY_RainbowIntensity": "rainbowIntensity"}
COLOR_FIELDS = {"CZY_HorizonColor": "skyHorizonColor", "CZY_ZenithColor": "skyZenithColor",
                "CZY_FilterColor": "filterColor", "CZY_SunHaloColor": "sunFlareColor",
                "CZY_SunFilterColor": "sunFilter", "CZY_SunColor": "sunColor",
                "CZY_MoonFlareColor": "moonFlareColor", "CZY_StarColor": "starColor",
                "CZY_GalaxyColor1": "galaxy1Color", "CZY_GalaxyColor2": "galaxy2Color",
                "CZY_GalaxyColor3": "galaxy3Color", "CZY_LightColumnColor": "lightScatteringColor"}
FACES = (((1, 0, 0), (0, 1, 0)), ((-1, 0, 0), (0, 1, 0)), ((0, 1, 0), (0, 0, -1)),
         ((0, -1, 0), (0, 0, 1)), ((0, 0, 1), (0, 1, 0)), ((0, 0, -1), (0, 1, 0)))
COZY_RUNTIME_SHA256 = "0063cffd3e4a9714c5ebb5dc14f7e14fc858a547259ac2f912137ede21b0a18c"


def capture_globals(path, values, day_fraction, mode):
    if not math.isfinite(day_fraction) or not 0 <= day_fraction < 1:
        raise ValueError(f"Invalid original atmosphere clock: {mode}")
    hours, minutes = divmod(round(day_fraction * 1440) % 1440, 60)
    expected_clock = re.compile(rf"\bhours={hours} minutes={minutes}\b")
    colors = {binding: tuple(values[field][channel] for channel in "rgba") for binding, field in COLOR_FIELDS.items()}
    for line in path.read_text(encoding="utf-8").splitlines():
        if not expected_clock.search(line) or "shader=Distant Lands/Cozy/URP/Stylized Sky Desktop" not in line:
            continue
        fields = dict(re.findall(r"global:(\w+)ID=(?:RGBA)?\(([^)]+)\)", line))
        shader = {key: tuple(map(float, entry.split(","))) for key, entry in fields.items()}
        stamp = re.search(r"\bt=(\d+(?:\.\d+)?)", line)
        if stamp and all(len(shader.get(key, ())) == 4 and
                         all(math.isclose(a, b, rel_tol=0, abs_tol=2e-6) for a, b in zip(expected, shader[key]))
                         for key, expected in colors.items()):
            return float(stamp.group(1)), shader
    raise ValueError(f"Original desktop sky capture does not match the clock and colors for {mode}")


def replay_globals(values, sun_day_fraction, seconds):
    if seconds is None or not math.isfinite(seconds) or seconds < 0 or not 0 <= sun_day_fraction < 1:
        raise ValueError("Invalid controlled sky clock")
    yaw, pitch = map(math.radians, (values["sunDirection"], values["sunPitch"]))
    if not all(map(math.isfinite, (yaw, pitch))):
        raise ValueError("Invalid original solar orientation")
    elevation = math.tau * sun_day_fraction - math.pi / 2
    # CozyWeather sets the child X angle and the parent Y and Z angles,
    # then publishes the negative world-space forward vector.
    x = math.sin(elevation) * math.sin(pitch)
    y = -math.sin(elevation) * math.cos(pitch)
    z = math.cos(elevation)
    sun = (-math.cos(yaw) * x - math.sin(yaw) * z, -y, math.sin(yaw) * x - math.cos(yaw) * z, 0)
    globals_ = {binding: tuple(values[field][channel] for channel in "rgba")
                for binding, field in COLOR_FIELDS.items()}
    globals_["CZY_SunDirection"] = sun
    # The original runtime registers this shader ID without a global setter.
    globals_["CZY_EclipseDirection"] = (0, 0, 0, 0)
    return seconds, globals_


def stage_constants(stage, name, values):
    shader, = [item for item in stage["constantBuffers"] if item["name"] == name]
    result = bytearray(shader["bytes"])
    for field in (*shader["vectors"], *shader["matrices"]):
        values_for_field = tuple(values[field["name"]])
        expected = field["dimension"] * (4 if field in shader["matrices"] else 1)
        if len(values_for_field) != expected:
            raise ValueError(f"Original shader binding width differs: {field['name']}")
        pack_into(f"<{expected}f", result, field["offset"], *values_for_field)
    return result


def view_projection(forward, up):
    right = np.cross(up, forward)
    view = np.eye(4)
    view[:3, :3] = (right, up, forward)
    near, far = .05, 1500
    projection = np.array(((1, 0, 0, 0), (0, 1, 0, 0), (0, 0, far / (far - near), -near * far / (far - near)),
                           (0, 0, 1, 0)))
    return (projection @ view).T.flatten()


def unity_rotation(scene, renderer):
    transform = scene.read(renderer["transform"])
    matrix = np.eye(4)
    while True:
        q = transform["m_LocalRotation"]
        x, y, z, w = (q[key] for key in "xyzw")
        local = np.eye(4)
        local[:3, :3] = ((1 - 2 * (y*y + z*z), 2 * (x*y - z*w), 2 * (x*z + y*w)),
                        (2 * (x*y + z*w), 1 - 2 * (x*x + z*z), 2 * (y*z - x*w)),
                        (2 * (x*z - y*w), 2 * (y*z + x*w), 1 - 2 * (x*x + y*y)))
        matrix = local @ matrix
        parent = transform["m_Father"]["fileID"]
        if not parent:
            return matrix
        transform = scene.read(parent)


def prepare(source, catalog, references, programs, original_log, output, mode="Sunset", *,
            replay_time=None, cozy_assembly=None):
    captured = original_log is not None
    replayed = replay_time is not None and cozy_assembly is not None
    if captured == replayed or (replay_time is not None) != (cozy_assembly is not None):
        raise ValueError("Choose one desktop log or a controlled time with its original assembly")
    recipe = json.loads(references.read_bytes())
    program_file = programs / "windows-atmosphere-programs.json"
    if recipe["d3d11ProgramSHA256"] != digest(program_file) or recipe["sceneSHA256"] != digest(source / ROOM_SCENE):
        raise ValueError("The sky requires the source-locked Windows scene and Direct3D programs")
    sky = json.loads(program_file.read_bytes())["programs"]["sky"]["stages"]
    draw = recipe["base"]["drawables"]["skyMesh"]
    mesh_entry = json.loads((catalog / "static-mesh-catalog/mesh-catalog.json").read_bytes())["slots"][draw["mesh"]["guid"]]
    mesh_path = source / mesh_entry["source"]
    if digest(mesh_path) != mesh_entry["sourceSHA256"]:
        raise ValueError("The Windows atmosphere sphere mesh differs")
    mesh = read_unity_mesh(mesh_path)
    vertex_data = {key: np.asarray(rows) for key, _, rows in attributes(mesh)}
    positions, normals, uv = (vertex_data[key] for key in ("POSITION", "NORMAL", "TEXCOORD_0"))
    # Return the conversion tool's right-handed positions, winding and UVs to their Unity inputs.
    positions[:, 2] *= -1
    normals[:, 2] *= -1
    uv[:, 1] = 1 - uv[:, 1]
    vertices = np.concatenate((positions, normals, uv), axis=1).astype("<f4")
    triangles, = indices(mesh, len(vertices), mesh["m_SubMeshes"])
    triangles = np.asarray(triangles, dtype="<u4").reshape(-1, 3)[:, [0, 2, 1]]
    world = unity_rotation(UnityScene(source / ROOM_SCENE), draw)
    object_to_world = stage_constants(sky["vertex"], "UnityPerDraw", {"unity_ObjectToWorld": world.T.flatten()})
    world_to_object = stage_constants(sky["fragment"], "UnityPerDraw",
                                      {"unity_WorldToObject": np.linalg.inv(world).T.flatten()})
    objects = bytes(a | b for a, b in zip(object_to_world, world_to_object, strict=True))

    reference = recipe["windowsReference"][mode]
    snapshot = reference["data"]
    if captured:
        seconds, globals_ = capture_globals(original_log, snapshot, recipe["modes"][mode]["dayFraction"], mode)
        provenance = {"input": "captured-desktop-globals", "logSHA256": digest(original_log)}
    else:
        if digest(cozy_assembly) != COZY_RUNTIME_SHA256:
            raise ValueError("Controlled sky replay requires the original Recovery Windows atmosphere assembly")
        seconds, globals_ = replay_globals(snapshot, recipe["modes"][mode]["sunDayFraction"], replay_time)
        provenance = {"input": "controlled-original-program-replay", "cozyAssemblySHA256": COZY_RUNTIME_SHA256,
                      "sunDayFraction": recipe["modes"][mode]["sunDayFraction"]}
    fields = {name: (snapshot[key],) for name, key in SCALAR_FIELDS.items()}
    fields.update({name: globals_[name] for name in COLOR_FIELDS})
    fields.update({"CZY_SunDirection": globals_["CZY_SunDirection"][:3],
                   "CZY_MoonDirection": tuple(snapshot["moonDirection"][axis] for axis in "xyz"),
                   "CZY_EclipseDirection": globals_["CZY_EclipseDirection"][:3],
                   "_Time": (seconds / 20, seconds, 2 * seconds, 3 * seconds),
                   "_TimeParameters": (seconds, np.sin(seconds), np.cos(seconds), 0),
                   "_WorldSpaceCameraPos": (0, 0, 0), "_UnderwaterRenderingEnabled": (0,), "_FullySubmerged": (0,)})
    output.mkdir()
    vertices.tofile(output / "vertices.bin")
    triangles.tofile(output / "indices.bin")
    (output / "object.bin").write_bytes(objects)
    (output / "world.bin").write_bytes(stage_constants(sky["fragment"], "$Globals", fields))
    for stage in sky.values():
        shader = programs / stage["path"]
        if digest(shader) != stage["sha256"]:
            raise ValueError("Original Windows sky program differs")
        (output / shader.name).write_bytes(shader.read_bytes())
    for i, (forward, up) in enumerate(FACES):
        constants = stage_constants(sky["vertex"], "$Globals", {"_ProjectionParams": (1, .05, 1500, 1 / 1500),
                                    "unity_MatrixVP": view_projection(forward, up)})
        (output / f"view-{i}.bin").write_bytes(constants)
    textures = {}
    for texture in sky["fragment"]["textures"]:
        slot, binding = texture["binding"], texture["name"]
        srgb = False
        if binding == "_UnderwaterMask":
            image = Image.new("RGBA", (1, 1))
        else:
            item = recipe["base"]["textures"][SKY_TEXTURES[binding]]
            with Image.open(verified_path(catalog / "image-catalog", item)) as pixels:
                image = pixels.convert("RGBA")
            srgb = item["srgb"]
        (output / f"texture-{slot}.txt").write_text(f"{image.width} {image.height} {int(srgb)}\n", encoding="ascii")
        pixels = image.tobytes()
        compressed = output / f"texture-{slot}.rgba.zlib"
        compressed.write_bytes(zlib.compress(pixels))
        textures[str(slot)] = {"width": image.width, "height": image.height, "srgb": srgb,
                               "pixelSHA256": sha256(pixels).hexdigest(), "fileSHA256": digest(compressed)}
    return {"mode": mode, "referenceSHA256": reference["sha256"], "vertexCount": len(vertices),
            "triangleCount": len(triangles), "sourceSeconds": seconds,
            "sunDirection": fields["CZY_SunDirection"], "meshSHA256": mesh_entry["sourceSHA256"],
            "programSHA256": {key: value["sha256"] for key, value in sky.items()}, **provenance,
            "worldConstantsSHA256": digest(output / "world.bin"),
            "textures": textures}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("source", "catalog", "references", "programs", "output"):
        parser.add_argument("--" + option, type=Path, required=True)
    inputs = parser.add_mutually_exclusive_group(required=True)
    inputs.add_argument("--original-log", type=Path)
    inputs.add_argument("--replay-time", type=float)
    parser.add_argument("--cozy-assembly", type=Path)
    parser.add_argument("--mode", choices=MODES, default="Sunset")
    args = parser.parse_args()
    if (args.replay_time is not None) != (args.cozy_assembly is not None):
        parser.error("--replay-time and --cozy-assembly are used together")
    result = prepare(args.source, args.catalog, args.references, args.programs, args.original_log, args.output, args.mode,
                     replay_time=args.replay_time, cozy_assembly=args.cozy_assembly)
    (args.output / "preparation.json").write_text(json.dumps(result) + "\n", encoding="utf-8")
    print(json.dumps(result))
