"""Compare snapshot normals with physical normal maps at a bounded shadow edge."""
import argparse
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
from native_render.light_lab import generate


def run(player, output, expected_validation=None, quality="existing"):
    output.mkdir(parents=True, exist_ok=False)
    report = dict(status="running", quality=quality, playerSha256=hashlib.sha256(player.read_bytes()).hexdigest(), captures=[])
    images = {}
    try:
        for name in ("surface", "screen", "geometry", "radial-geometry", "fixed", "document"):
            print(name, flush=True)
            folder = output / name
            generate(folder, "directional-near")
            Image.new("RGBA", (2, 2), (230, 128, 205, 255)).save(folder / "normal.png")
            Image.new("RGBA", (2, 1), (255, 255, 255, 255)).save(folder / "ramp.png")
            model = json.loads((folder / "plane.gltf").read_bytes())
            model.update(images=[dict(uri="normal.png"), dict(uri="ramp.png")], textures=[dict(source=0), dict(source=1)])
            receiver = model["materials"][0]
            receiver["normalTexture"] = dict(index=0)
            receiver["pbrMetallicRoughness"].update(baseColorFactor=[.4, .3, .2, 1], metallicFactor=0, roughnessFactor=1)
            receiver["extras"] = dict(azureSlotId="surface", azureRenderMaterial=dict(schemaVersion=2, **{"class": "generic"},
                features=["diffuse-ramp"], diffuseRamp=dict(schemaVersion=1, texture=1), styleParameters=[1, 1, 0, 0],
                nprDiffuse=dict(schemaVersion=1, shadowScale=0, shadowAlbedo="base-color", lightingInputs="host-scene-radiance",
                    shadowMask="one", baseAo="one", characterVisibility="object-shadow")))
            screen = name in ("screen", "fixed", "document")
            if screen:
                receiver.pop("normalTexture")
                receiver["extras"]["azureRenderMaterial"]["nprDiffuse"]["lightingNormal"] = "screen-surface"
            if name == "radial-geometry":
                npr = receiver["extras"]["azureRenderMaterial"]["nprDiffuse"]
                npr.update(lightingNormal="origin-radial-horizontal", rimOrigin=dict(schemaVersion=1, node="Origin"))
            plain = dict(pbrMetallicRoughness=dict(baseColorFactor=[0, 0, 0, 1], metallicFactor=0, roughnessFactor=1))
            model["materials"] += [copy.deepcopy(plain), dict(**plain, normalTexture=dict(index=0))]
            for index in (1, 2):
                mesh = copy.deepcopy(model["meshes"][0])
                mesh["primitives"][0]["material"] = index
                model["meshes"].append(mesh)
            model["nodes"] += [dict(mesh=1, translation=[0, 0, 1], scale=[.125, .125, 1]),
                dict(mesh=2, translation=[0, 0, -.01]), dict(name="Origin")]
            model["scenes"][0]["nodes"] = [0, 1, 2, 3]
            (folder / "plane.gltf").write_text(json.dumps(model), encoding="utf-8")
            level = json.loads((folder / "level.azurelevel").read_bytes())
            level["lights"][0]["shape"]["direction"] = [.6, 0, -.8]
            level["nodes"][0]["components"] = {"azure.object-shadow": dict(type="azure.object-shadow", version=1, data=dict(
                enabled=True, center=[0, 0, .5], halfExtents=[6, 6, 3], normalBias=.1, lightBias=.05,
                filterRadiusX=4, filterRadiusY=2, atlasPage=6, surfaceNormalBias=name != "geometry"))}
            level["renderSettings"].update(shadowDistance=20, nprLighting=dict(schemaVersion=1, enabled=True,
                ambientColor=[1, 1, 1], ambientDirection=[0, 0, 1], directionBias=0, directionGain=0, directionFloor=1,
                ambientWeight=1, litAmbientMix=0, mainColorMix=0, shadowOverride=1, outdoorGain=1,
                shadowAmbientGain=1, indoorMix=0, useSceneIrradiance=False))
            if quality != "existing":
                level["renderSettings"].update(schemaVersion=19, antiAliasing=4 if quality == "multisample" else 2)
            if name == "document":
                ids = {str(index): str(uuid.uuid4()) for index in (0, 1)}
                identity = str(uuid.uuid4())
                for index, key in enumerate(("normal", "ramp")):
                    (folder / (key + ".png.azmeta")).write_text(json.dumps(dict(schemaVersion=1, id=ids[str(index)], settings={}, dependencies=[])), encoding="utf-8")
                (folder / "surface.azurematerial").write_text(json.dumps(complete_material_document(receiver, model, ids)), encoding="utf-8")
                (folder / "surface.azurematerial.azmeta").write_text(json.dumps(dict(schemaVersion=1, id=identity, settings={}, dependencies=list(ids.values()))), encoding="utf-8")
                level["nodes"][0]["materialSlots"] = {"surface": identity}
            (folder / "level.azurelevel").write_text(json.dumps(level), encoding="utf-8")
            command = [str(player.resolve()), "--project", str(folder / "lab.azureproject"), "--width", "256", "--height", "256",
                "--capture-dir", str(folder / "frames"), "--capture-frames", "1", "--fixed-frame-step", "--qa-camera", "scene", "--qa-light", "scene", "--qa-isolation", "ambient"]
            if name == "fixed":
                command += ["--disable-bindless", "--disable-compute-skinning", "--disable-parallel-recording"]
            (folder / "command.json").write_text(json.dumps(command, indent=2), encoding="utf-8")
            startup = None
            if hasattr(subprocess, "STARTUPINFO"):
                startup = subprocess.STARTUPINFO()
                startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
                startup.wShowWindow = 0
            with (folder / "player.log").open("wb") as log:
                subprocess.run(command, stdout=log, stderr=log, startupinfo=startup, timeout=300, check=True)
            text = (folder / "player.log").read_text(encoding="utf-8", errors="replace")
            assert not re.search(r"VUID-|SYNC-HAZARD|Validation (?:Error|Warning)|\[error\]", text), name
            assert "Allocator after unload: buffers=0 images=0" in text, name
            binding = "fixed" if name == "fixed" else "indexed"
            assert "Character texture access: " + binding in text, name
            validation = "Validation layer: enabled" in text
            if expected_validation:
                assert validation == (expected_validation == "enabled"), name
            manifest = json.loads((folder / "frames/capture_manifest.json").read_bytes())
            assert manifest["objectShadowPages"] == 1 and manifest["reflectionSurfaceSnapshot"] == screen, name
            assert manifest["rasterizationSamples"] == (4 if quality == "multisample" else 1), name
            assert manifest["antiAliasing"] == {"existing": 0, "multisample": 4, "supersample": 2}[quality], name
            extent = 512 if quality == "supersample" else 256
            assert manifest["sceneRenderWidth"] == manifest["sceneRenderHeight"] == extent, name
            assert manifest["snapshotRasterizationSamples"] == (4 if quality == "multisample" else 1), name
            assert not manifest["multisampleFallback"], name
            image = np.asarray(Image.open(folder / "frames/frame_000000.png").convert("RGB"))
            images[name] = image
            report["captures"].append(dict(name=name, validationEnabled=validation, rasterizationSamples=manifest["rasterizationSamples"],
                imageSha256=hashlib.sha256((folder / "frames/frame_000000.png").read_bytes()).hexdigest()))
        differences = {}
        for name in ("screen", "fixed", "document"):
            delta = np.abs(images[name].astype(int) - images["surface"].astype(int))
            differences[name] = dict(maximum=int(delta.max()), pixelsOverOne=int((delta.max(2) > 1).sum()))
            assert delta.max() <= 1, (name, differences[name])
        assert np.array_equal(images["radial-geometry"], images["geometry"])
        delta = np.abs(images["surface"].astype(int) - images["geometry"].astype(int)).max(2)
        assert (delta > 2).sum() > 20, "The independent normal control must exercise shadow bias"
        report.update(status="passed", comparisons=differences, normalControlChangedPixels=int((delta > 2).sum()))
    except Exception as error:
        report.update(status="failed", error=str(error))
        raise
    finally:
        (output / "summary.json").write_text(json.dumps(report, indent=2), encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--expected-validation", choices=("enabled", "disabled"))
    parser.add_argument("--quality", choices=("existing", "multisample", "supersample"), default="existing")
    args = parser.parse_args()
    if args.output:
        run(args.executable, args.output.resolve(), args.expected_validation, args.quality)
    else:
        base = Path(__file__).resolve().parents[1] / "build/runs"
        with tempfile.TemporaryDirectory(prefix="screen-normal-shadow-", dir=base) as temporary:
            folder = Path(temporary).resolve()
            assert folder.parent == base.resolve()
            run(args.executable, folder / "capture", args.expected_validation, args.quality)
