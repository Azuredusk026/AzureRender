"""Check depth-owned screen normals against an independent NPR rim reference."""
import argparse
import base64
import copy
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile
import uuid

import numpy as np
from PIL import Image

from convert_material_semantics import complete_material_document
from test_material_origin_gpu import linear, prepare, reference, unit


def snapshot_reference(pixel, origin_x, light):
    position = np.array([(pixel[0] + .5) / 128 - 1, 1 - (pixel[1] + .5) / 128, 0])
    xy = np.array([230, 200]) / 255 * 2 - 1
    normal = unit([*xy, np.sqrt(max(1 - np.dot(xy, xy), 0))])
    view = unit(np.array([0, 0, 5]) - position)
    incoming = np.asarray(light) - position
    direction = unit(incoming)
    planar = unit([direction[0], direction[1], 0])
    radial = position - [origin_x, 0, 0]
    radial[1] = 1 / 16384
    edge = 1 - abs(np.dot(normal, view))
    band = np.clip((edge - .62) / .16, 0, 1)
    band = band * band * (3 - 2 * band)
    visibility = np.clip(np.dot(unit(radial), direction) + 1, 0, 1)
    attenuation = (1 - min(np.dot(incoming, incoming) / 100, 1)) ** 2
    return np.full(3, .5 * band * visibility * max(np.dot(normal, planar), 0) * .1 * attenuation * 3)


def run(player, output, quality="existing", cases=None, expected_validation=None):

    output.mkdir(parents=True, exist_ok=False)
    report = dict(status="running", quality=quality, captures=[], executableSha256=hashlib.sha256(player.read_bytes()).hexdigest())
    frames = {}
    try:
        names = ("screen", "omitted", "fixed", "document", "projected", "empty", "opaque", "mask", "hair-default", "hair-normal", "hair-fixed", "hair-document", "hair-beauty-default", "hair-beauty-normal")
        if cases:
            assert set(cases) <= set(names), "Unknown screen normal case"
            names = tuple(name for name in names if name in cases)
        for name in names:
            folder = output / name
            pixel, origin, translation, scale, light = prepare(folder, "left")
            model = json.loads((folder / "plane.gltf").read_bytes())
            payload = bytearray(base64.b64decode(model["buffers"][0]["uri"].split(",", 1)[1]))
            def append(values, kind):
                values = np.ascontiguousarray(values, dtype="<f4")
                offset = len(payload)
                payload.extend(values.tobytes())
                view = len(model["bufferViews"])
                model["bufferViews"].append(dict(buffer=0, byteOffset=offset, byteLength=values.nbytes))
                index = len(model["accessors"])
                entry = dict(bufferView=view, componentType=5126, count=len(values), type=kind)
                if kind == "VEC3":
                    entry.update(min=values.min(0).tolist(), max=values.max(0).tolist())
                model["accessors"].append(entry)
                return index
            background = copy.deepcopy(model["meshes"][0])
            background["primitives"][0]["material"] = 1
            primitive = model["meshes"][0]["primitives"][0]
            for key, values, kind in (
                    ("NORMAL", np.tile([0, 0, 1], (4, 1)).astype("<f4"), "VEC3"),
                    ("TANGENT", np.tile([1, 0, 0, 1], (4, 1)).astype("<f4"), "VEC4")):
                primitive["attributes"][key] = append(values, kind)
            model["meshes"].append(background)
            model["nodes"].append(dict(mesh=1, translation=[0, 0, -.1]))
            model["scenes"][0]["nodes"].append(2)
            if name == "projected":
                original_positions = np.frombuffer(payload[:48], dtype="<f4").reshape(4, 3).copy()
                left = original_positions.copy()
                left[:, 0] = (left[:, 0] - 4) * .5
                background["primitives"][0]["attributes"]["POSITION"] = append(left, "VEC3")
                right_mesh = copy.deepcopy(background)
                right = original_positions.copy()
                right[:, 0] = (right[:, 0] + 4) * .5
                right_mesh["primitives"][0]["attributes"]["POSITION"] = append(right, "VEC3")
                normal = unit([-.98, 0, .2])
                right_mesh["primitives"][0]["attributes"]["NORMAL"] = append(np.tile(normal, (4, 1)), "VEC3")
                right_mesh["primitives"][0]["attributes"]["TANGENT"] = append(np.tile([normal[2], 0, -normal[0], 1], (4, 1)), "VEC4")
                model["meshes"].append(right_mesh)
                model["nodes"].append(dict(mesh=2, translation=[0, 0, -.1]))
                model["scenes"][0]["nodes"].append(3)
                primitive["attributes"]["TEXCOORD_1"] = append(np.tile([1, 0], (4, 1)), "VEC2")
            if name == "empty":
                primitive["attributes"]["NORMAL"] = background["primitives"][0]["attributes"]["NORMAL"]
                primitive["attributes"]["TANGENT"] = background["primitives"][0]["attributes"]["TANGENT"]
                model["extensionsUsed"] = ["AZURE_node_opacity"]
                model["nodes"][2]["extensions"] = {"AZURE_node_opacity": dict(schemaVersion=1, opacity=0)}
            model["buffers"][0] = dict(byteLength=len(payload), uri="data:application/octet-stream;base64," + base64.b64encode(payload).decode())
            hair = name.startswith("hair-")
            beauty = "beauty" in name
            if hair:
                background["primitives"][0]["attributes"]["NORMAL"] = primitive["attributes"]["NORMAL"]
                background["primitives"][0]["attributes"]["TANGENT"] = primitive["attributes"]["TANGENT"]
            receiver = model["materials"][0]
            receiver["alphaMode"] = "BLEND"
            npr = receiver["extras"]["azureRenderMaterial"]["nprDiffuse"]
            npr["transparentHighlights"] = True
            if name in ("opaque", "mask"):
                receiver["alphaMode"] = name.upper()
                npr["transparentHighlights"] = False
            if name == "projected":
                receiver["extras"]["azureRenderMaterial"]["outlineSurface"] = dict(
                    schemaVersion=1, mode="view-scaled", target="surface", width=8,
                    minimumPixels=.5, normalSource="tangent-uv1")
            if name != "omitted":
                npr["lightingNormal"] = "screen-surface"
            model["materials"].append(dict(name="normal-producer", pbrMetallicRoughness=dict(
                baseColorFactor=[.3, .3, .3, 1], metallicFactor=0, roughnessFactor=1)))
            if hair:
                Image.new("RGBA", (4, 4), (230, 200, 255, 255)).save(folder / "normal.png")
                Image.new("RGBA", (4, 4), (128, 128, 0, 255)).save(folder / "parameter.png")
                model["images"] += [dict(uri="normal.png"), dict(uri="parameter.png")]
                model["textures"] += [dict(source=1), dict(source=2)]
                producer = model["materials"][1]
                producer["normalTexture"] = dict(index=1)
                ramp = dict(schemaVersion=1, parameterTexture=2, rampTexture=0,
                    primaryShift=0, secondaryShift=.01, intensity=1, secondaryRange=.5,
                    edgeFade=1, specularNormalScale=1, surfaceNormalScale=1, directionX=0,
                    secondaryColor=[1, 1, 1], objectBasis=[1, 0, 0, 0, 1, 0, 0, 0, 1],
                    tangentSign=1, surfaceResponse="strand-diffuse")
                if name not in ("hair-default", "hair-beauty-default"):
                    ramp["snapshotNormal"] = "material-normal-rg"
                producer["extras"] = dict(azureSlotId="hair", azureRenderMaterial=dict(
                    schemaVersion=2, **{"class": "hair"}, features=["hair-anisotropy", "hair-projected-ramp"],
                    hair=dict(power=32, strength=1, rampRow=0, strandShift=0, dataTexture=2, projectedRamp=ramp)))
                if beauty:
                    npr.pop("lightingNormal")
                    model["extensionsUsed"] = ["AZURE_node_opacity"]
                    model["nodes"][0]["extensions"] = {"AZURE_node_opacity": dict(schemaVersion=1, opacity=0)}
            (folder / "plane.gltf").write_text(json.dumps(model), encoding="utf-8")
            level = json.loads((folder / "level.azurelevel").read_bytes())
            if hair:
                light = [2, 1, 1]
                level["nodes"][2]["components"]["azure.transform"]["data"]["translation"] = light
            if beauty:
                level["lights"][0]["shape"].pop("characterResponse")
                level["lights"][0]["intensity"] = 1
            if name == "hair-document":
                image_ids = {str(i): str(uuid.uuid4()) for i in range(3)}
                material_id = str(uuid.uuid4())
                for i, image in enumerate(model["images"]):
                    (folder / (image["uri"] + ".azmeta")).write_text(json.dumps(dict(schemaVersion=1,
                        id=image_ids[str(i)], dependencies=[], settings={})), encoding="utf-8")
                (folder / "hair.azurematerial").write_text(json.dumps(complete_material_document(
                    model["materials"][1], model, image_ids)), encoding="utf-8")
                (folder / "hair.azurematerial.azmeta").write_text(json.dumps(dict(schemaVersion=1,
                    id=material_id, dependencies=list(image_ids.values()), settings={})), encoding="utf-8")
                level["nodes"][0]["materialSlots"] = {"hair": material_id}
            if name == "document":
                image_id, material_id = str(uuid.uuid4()), str(uuid.uuid4())
                (folder / "lut.png.azmeta").write_text(json.dumps(dict(schemaVersion=1, id=image_id, dependencies=[], settings={})), encoding="utf-8")
                (folder / "surface.azurematerial").write_text(json.dumps(
                    complete_material_document(receiver, model, {"0": image_id})), encoding="utf-8")
                (folder / "surface.azurematerial.azmeta").write_text(json.dumps(dict(schemaVersion=1, id=material_id, dependencies=[image_id], settings={})), encoding="utf-8")
                level["nodes"][0]["materialSlots"] = {"surface": material_id}
            if quality != "existing":
                level["renderSettings"]["schemaVersion"] = 19
            level["renderSettings"]["antiAliasing"] = 4 if quality == "multisample" else 2 if quality == "supersample" else 0
            (folder / "level.azurelevel").write_text(json.dumps(level), encoding="utf-8")
            command = [str(player), "--project", str(folder / "lab.azureproject"), "--width", "256", "--height", "256",
                       "--capture-dir", str(folder / "frames"), "--capture-frames", "1", "--fixed-frame-step",
                       "--qa-camera", "scene", "--qa-light", "scene", "--qa-isolation", "beauty" if beauty else "rim"]
            if name in ("fixed", "hair-fixed"):
                command += ["--disable-bindless", "--disable-compute-skinning", "--disable-parallel-recording"]
            startup = None
            if hasattr(subprocess, "STARTUPINFO"):
                startup = subprocess.STARTUPINFO()
                startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
                startup.wShowWindow = 0
            with (folder / "player.log").open("wb") as log:
                subprocess.run(command, stdout=log, stderr=log, startupinfo=startup, timeout=240, check=True)
            log = (folder / "player.log").read_text(encoding="utf-8", errors="replace")
            assert not re.search(r"VUID-|SYNC-HAZARD|Validation (?:Error|Warning)|\[error\]", log), name
            assert "Allocator after unload: buffers=0 images=0" in log, name
            validation = "Validation layer: enabled" in log
            if expected_validation is not None:
                assert validation == (expected_validation == "enabled"), name
            binding = "fixed" if name in ("fixed", "hair-fixed") else "indexed"
            assert "Character texture access: " + binding in log, name
            manifest = json.loads((folder / "frames/capture_manifest.json").read_bytes())
            assert manifest["reflectionSurfaceSnapshot"] == (name != "omitted" and not beauty), name
            assert manifest["rasterizationSamples"] == (4 if quality == "multisample" else 1), name
            image = np.asarray(Image.open(folder / "frames/frame_000000.png").convert("RGB"))
            source_pixel = [pixel[0] - 2.56, pixel[1]] if name == "projected" else pixel
            expected = reference(source_pixel, origin, translation, scale, light) if name != "omitted" else np.zeros(3)
            if hair:
                expected = snapshot_reference(pixel, origin, light) if name not in ("hair-default", "hair-beauty-default") else np.zeros(3)
            actual = linear(image[pixel[1], pixel[0]])
            if not beauty:
                assert np.max(np.abs(actual - expected)) <= .008, (name, actual, expected)
            frames[name] = image
            report["captures"].append(dict(name=name, actual=actual.tolist(), expected=None if beauty else expected.tolist(),
                comparison="pairwise" if beauty else "independent-linear-rim",
                maximumLinearError=None if beauty else float(np.max(np.abs(actual-expected))),
                validationEnabled=validation, descriptorBinding=binding, rasterizationSamples=manifest["rasterizationSamples"]))
        for first, second in (("screen", "fixed"), ("screen", "document"), ("screen", "opaque"), ("screen", "mask"),
                              ("hair-normal", "hair-fixed"), ("hair-normal", "hair-document"),
                              ("hair-beauty-default", "hair-beauty-normal")):
            if first in frames and second in frames:
                assert np.array_equal(frames[first], frames[second]), (first, second)
        if "screen" in frames and "omitted" in frames:
            assert np.max(frames["screen"][128, 128]) > 10 and np.max(frames["omitted"][128, 128]) == 0
        if "hair-beauty-normal" in frames:
            assert np.max(frames["hair-beauty-normal"][128, 128]) > 20
        assert hashlib.sha256(player.read_bytes()).hexdigest() == report["executableSha256"]
        report["status"] = "passed"
        print(json.dumps(dict(status="passed", cases=len(report["captures"]), quality=quality)), flush=True)
    finally:
        (output / "summary.json").write_text(json.dumps(report, indent=2), encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--quality", choices=("existing", "multisample", "supersample"), default="existing")
    parser.add_argument("--case", action="append")
    parser.add_argument("--expected-validation", choices=("enabled", "disabled"))
    args = parser.parse_args()
    if args.output:
        run(args.executable.resolve(), args.output.resolve(), args.quality, args.case, args.expected_validation)
    else:
        with tempfile.TemporaryDirectory(prefix="screen-normal-") as directory:
            run(args.executable.resolve(), Path(directory) / "captures", args.quality, args.case, args.expected_validation)
