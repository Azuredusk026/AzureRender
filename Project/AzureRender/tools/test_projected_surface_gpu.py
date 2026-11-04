"""Verify material-surface projection using independent silhouette bounds."""
import argparse
import base64
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile
import uuid

import numpy as np
from PIL import Image

from native_render.light_lab import generate
from convert_material_semantics import complete_material_document
from test_outline_projection_gpu import reference_bounds, sphere


def run(player, output, quality="existing", selected=None):
    output.mkdir(parents=True, exist_ok=False)
    digest = hashlib.sha256(player.read_bytes()).hexdigest()
    frames = {}
    rows = []
    report = dict(status="running", captures=rows, playerSha256=digest, quality=quality)
    try:
        names = ("zero", "wide", "fixed", "fixed-compute", "indexed-vertex", "document", "blend", "near", "resize", "morph", "morph-fixed", "translated")
        if selected:
            assert set(selected) <= set(names), selected
            names = tuple(name for name in names if name in selected)
        for name in names:
            folder = output / name
            generate(folder, "directional-near")
            model, payload, positions = sphere()
            primitive = model["meshes"][0]["primitives"][0]
            for name_, values, component, kind in (
                    ("JOINTS_0", np.zeros((len(positions), 4), dtype="<u2"), 5123, "VEC4"),
                    ("WEIGHTS_0", np.tile([1, 0, 0, 0], (len(positions), 1)).astype("<f4"), 5126, "VEC4")):
                offset = len(payload)
                payload.extend(values.tobytes())
                view = len(model["bufferViews"])
                model["bufferViews"].append(dict(buffer=0, byteOffset=offset, byteLength=values.nbytes))
                index = len(model["accessors"])
                model["accessors"].append(dict(bufferView=view, componentType=component, count=len(positions), type=kind))
                primitive["attributes"][name_] = index
            model["nodes"][0]["skin"] = 0
            model["nodes"].append(dict(name="fixture-joint"))
            model["scenes"][0]["nodes"].append(1)
            model["skins"] = [dict(joints=[1])]
            if name.startswith("morph"):
                values = np.tile([.2, 0, 0], (len(positions), 1)).astype("<f4")
                offset = len(payload)
                payload.extend(values.tobytes())
                view = len(model["bufferViews"])
                model["bufferViews"].append(dict(buffer=0, byteOffset=offset, byteLength=values.nbytes))
                index = len(model["accessors"])
                model["accessors"].append(dict(bufferView=view, componentType=5126, count=len(positions),
                                              type="VEC3", min=[.2, 0, 0], max=[.2, 0, 0]))
                zeros = np.zeros_like(values)
                offset = len(payload)
                payload.extend(zeros.tobytes())
                view = len(model["bufferViews"])
                model["bufferViews"].append(dict(buffer=0, byteOffset=offset, byteLength=zeros.nbytes))
                zero_index = len(model["accessors"])
                model["accessors"].append(dict(bufferView=view, componentType=5126, count=len(positions),
                                              type="VEC3", min=[0, 0, 0], max=[0, 0, 0]))
                primitive["targets"] = [dict(POSITION=zero_index), dict(POSITION=index)]
                model["meshes"][0]["weights"] = [0, 1]
                model["nodes"][0]["weights"] = [0, 1]
            model["buffers"] = [dict(byteLength=len(payload), uri="data:application/octet-stream;base64," + base64.b64encode(payload).decode())]
            Image.new("RGBA", (4, 4), (255, 255, 255, 255)).save(folder / "mask.png")
            Image.new("RGBA", (4, 4), (255, 0, 0, 255)).save(folder / "base.png")
            model["images"] = [dict(uri="mask.png"), dict(uri="base.png")]
            model["textures"] = [dict(source=0), dict(source=1)]
            width = 0 if name == "zero" else 4
            shape = dict(schemaVersion=1, mode="view-scaled", target="surface", width=width,
                         minimumPixels=.5, normalSource="geometry", maskTexture=0)
            model["materials"] = [dict(
                name="projected-material", alphaMode="BLEND" if name == "blend" else "OPAQUE",
                pbrMetallicRoughness=dict(baseColorTexture=dict(index=1), metallicFactor=0, roughnessFactor=1),
                extras=dict(azureRenderMaterial=dict(schemaVersion=2, **{"class": "generic"},
                                                     features=["unlit-surface"], surface=dict(schemaVersion=1, lighting="unlit"), outlineSurface=shape)))]
            (folder / "plane.gltf").write_text(json.dumps(model), encoding="utf-8")
            level = json.loads((folder / "level.azurelevel").read_bytes())
            camera = next(node for node in level["nodes"] if node["id"] == "camera")["components"]
            distance = 3 if name == "near" else 5
            camera["azure.transform"]["data"]["translation"] = [0, 0, distance]
            camera["azure.camera"]["data"] = dict(enabled=True, priority=1, fieldOfView=55, nearPlane=.1, farPlane=20)
            if quality != "existing":
                level["renderSettings"]["schemaVersion"] = 19
            if name == "document":
                image_ids = {str(i): str(uuid.uuid4()) for i in range(2)}
                material_id = str(uuid.uuid4())
                for i, image in enumerate(model["images"]):
                    (folder / (image["uri"] + ".azmeta")).write_text(json.dumps(dict(
                        schemaVersion=1, id=image_ids[str(i)], dependencies=[], settings={})), encoding="utf-8")
                (folder / "surface.azurematerial").write_text(json.dumps(
                    complete_material_document(model["materials"][0], model, image_ids)), encoding="utf-8")
                (folder / "surface.azurematerial.azmeta").write_text(json.dumps(dict(
                    schemaVersion=1, id=material_id, dependencies=list(image_ids.values()), settings={})), encoding="utf-8")
                model["materials"][0]["extras"]["azureSlotId"] = "projected"
                (folder / "plane.gltf").write_text(json.dumps(model), encoding="utf-8")
                next(node for node in level["nodes"] if node.get("resourceId") == "plane")["materialSlots"] = {"projected": material_id}
            level["renderSettings"]["antiAliasing"] = 4 if quality == "multisample" else 2 if quality == "supersample" else 0
            (folder / "level.azurelevel").write_text(json.dumps(level), encoding="utf-8")
            if name == "translated":
                node = next(node for node in level["nodes"] if node.get("resourceId") == "plane")
                node["components"] = {"azure.transform": dict(type="azure.transform", version=1, data=dict(translation=[.2, 0, 0]))}
                (folder / "level.azurelevel").write_text(json.dumps(level), encoding="utf-8")
            size = 512 if name == "resize" else 256
            command = [str(player), "--project", str(folder / "lab.azureproject"), "--width", str(size),
                       "--height", str(size), "--capture-dir", str(folder / "frames"), "--capture-frames", "2",
                       "--fixed-frame-step", "--qa-camera", "scene", "--qa-light", "scene",
                       "--set", "render.features.showcase-platform=false", "--set", "render.features.face-sdf=false"]
            if name in ("fixed", "fixed-compute", "morph-fixed"):
                command += ["--disable-bindless"]
            if name in ("fixed", "indexed-vertex", "morph-fixed"):
                command += ["--disable-compute-skinning"]
            if name in ("fixed", "morph-fixed"):
                command += ["--disable-parallel-recording"]
            startup = None
            if hasattr(subprocess, "STARTUPINFO"):
                startup = subprocess.STARTUPINFO()
                startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
                startup.wShowWindow = 0
            with (folder / "player.log").open("wb") as log:
                subprocess.run(command, stdout=log, stderr=log, startupinfo=startup, timeout=240, check=True)
            log = (folder / "player.log").read_text(encoding="utf-8", errors="replace")
            assert "Allocator after unload: buffers=0 images=0" in log, name
            binding = "fixed" if name in ("fixed", "fixed-compute", "morph-fixed") else "indexed"
            skinning = "vertex-shader fallback path" if name in ("fixed", "indexed-vertex", "morph-fixed") else "Vulkan compute path"
            assert "Character texture access: " + binding in log, name
            assert "Character skinning and morph: " + skinning in log, name
            assert not re.search(r"VUID-|SYNC-HAZARD|Validation (?:Error|Warning)|\[error\]", log), name
            manifest = json.loads((folder / "frames/capture_manifest.json").read_bytes())
            assert manifest["rasterizationSamples"] == (4 if quality == "multisample" else 1), name
            image = np.asarray(Image.open(folder / "frames/frame_000001.png").convert("RGB"))
            red = (image[:, :, 0] > 250) & (image[:, :, 1] < 4) & (image[:, :, 2] < 4)
            assert red.sum() > 100, name
            ys, xs = np.where(red)
            deformed = np.asarray(positions) + ([.2, 0, 0] if name.startswith("morph") or name == "translated" else [0, 0, 0])
            expected = reference_bounds(deformed, size, distance, 55, width, normals=positions) if width else np.array([
                size / 2 - size / 2 / np.tan(np.deg2rad(55) / 2) / np.sqrt(distance * distance - 1),
                size / 2 + size / 2 / np.tan(np.deg2rad(55) / 2) / np.sqrt(distance * distance - 1)])
            actual = np.array([xs.min(), xs.max() + 1])
            assert np.max(np.abs(expected - actual)) <= 1.5, (name, expected, actual)
            frames[name] = image
            rows.append(dict(name=name, pixels=int(red.sum()), bounds=actual.tolist(), expected=expected.tolist()))
        for name in ("fixed", "fixed-compute", "indexed-vertex", "document", "blend"):
            if "wide" in frames and name in frames:
                assert np.array_equal(frames["wide"], frames[name]), ("Equivalent surface differs", name)
        for name in ("translated", "morph-fixed"):
            if "morph" in frames and name in frames:
                assert np.max(np.abs(frames["morph"].astype(int) - frames[name].astype(int))) <= 1, ("Morph path differs", name)
        counts = {row["name"]: row["pixels"] for row in rows}
        if "wide" in counts and "zero" in counts:
            assert counts["wide"] > counts["zero"], "Projection has no visible effect"
        assert hashlib.sha256(player.read_bytes()).hexdigest() == digest
        report["status"] = "passed"
        print(json.dumps(dict(status="passed", cases=len(rows), quality=quality)), flush=True)
    finally:
        (output / "summary.json").write_text(json.dumps(report, indent=2), encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--case", action="append")
    parser.add_argument("--quality", choices=("existing", "multisample", "supersample"), default="existing")
    args = parser.parse_args()
    if args.output:
        run(args.executable.resolve(), args.output.resolve(), args.quality, args.case)
    else:
        with tempfile.TemporaryDirectory(prefix="projected-surface-") as directory:
            run(args.executable.resolve(), Path(directory) / "captures", args.quality, args.case)
