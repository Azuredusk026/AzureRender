"""Verify procedural view masks and shadow saturation against linear color references."""
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

from convert_material_semantics import complete_material_document
from gpu_texture_access import require_texture_access
from test_npr_ramp_composition_gpu import BASE, RAMP, LUMA, linear, prepare, write


def reference(z, visibility, saturation=1, procedural=True, blend=False):
    lit = linear(BASE[:3]) * .96
    shadow = lit * .275
    luma = np.dot(shadow, LUMA)
    shadow = luma + (shadow - luma) * saturation
    alpha, ao = RAMP[3] / 255, BASE[3] / 255
    lit_mask = min(ao, alpha)
    t = np.clip((z - .25) / .75, 0, 1)
    view_mask = 1 if procedural is None else t * t * (3 - 2 * t) if procedural else alpha
    weight = ao * view_mask
    shade_mix = np.clip(weight + alpha, 0, 1)
    saturate = lambda color: np.dot(color, LUMA) + (color - np.dot(color, LUMA)) * 1.2
    shade = saturate(.65 * shadow) * (1 - shade_mix) + shadow * shade_mix
    untinted = shade * (1 - lit_mask) + lit * lit_mask
    ramp = RAMP[:3] / 255
    chroma = ramp.max() - ramp.min()
    tinted = untinted * (1 - chroma + ramp * chroma)
    outdoor = tinted * np.clip(np.dot(untinted, LUMA) / max(np.dot(tinted, LUMA), .001), 0, 1.5)
    indoor = shadow * (1 - weight) + saturate(lit) * weight
    color = indoor * (1 - visibility) + outdoor * visibility
    return color * (ao if blend else 1)


def run(player, output, cases=None, expected_validation=None, quality="existing"):
    output.mkdir(parents=True, exist_ok=False)
    report = dict(status="running", quality=quality, executableSha256=hashlib.sha256(player.read_bytes()).hexdigest(), captures=[])
    definitions = [(kind + "-" + str(visibility), kind, visibility, .6, 1, True)
        for kind in ("generic", "fabric") for visibility in (0, .5, 1)]
    definitions += [(name, "generic", .5, z, sat, procedural) for name, z, sat, procedural in (
        ("low", -.2, 1, True), ("front", 1, 1, True), ("saturated", .6, 1.1, True),
        ("shadow-only", .6, 1.1, False), ("shadow-default", .6, 1.1, None), ("fixed", .6, 1.1, True), ("document", .6, 1.1, True),
        ("blend", .6, 1.1, True))]
    if cases:
        assert set(cases) <= {row[0] for row in definitions}
        definitions = [row for row in definitions if row[0] in cases]
    images = {}
    try:
        for name, kind, visibility, z, saturation, procedural in definitions:
            print(name, flush=True)
            folder = output / name
            project = prepare(folder, kind, "view", visibility, "blend" if name == "blend" else "opaque")
            model = json.loads((folder / "plane.gltf").read_bytes())
            material = model["materials"][1]
            profile = material["extras"]["azureRenderMaterial"]
            npr = profile["nprDiffuse"]
            npr["rampComposition"] = "mask-visibility" if procedural is None else "view-facing-mask" if procedural else "view-ramp-alpha"
            if saturation != 1:
                npr["shadowSaturation"] = saturation
            # Vertex normals specify an exact independent camera angle.
            blob = bytearray(base64.b64decode(model["buffers"][0]["uri"].split(",", 1)[1]))
            normal = np.array([np.sqrt(1 - z * z), 0, z], dtype="<f4")
            blob[48:96] = np.tile(normal, (4, 1)).tobytes()
            model["buffers"][0]["uri"] = "data:application/octet-stream;base64," + base64.b64encode(blob).decode()
            level = json.loads((folder / "level.azurelevel").read_bytes())
            if quality == "multisample":
                level["renderSettings"].update(schemaVersion=19, antiAliasing=4)
            if name == "document":
                ids = {str(i): str(uuid.uuid4()) for i in range(2)}
                for index, key in enumerate(("base", "ramp")):
                    write(folder / (key + ".png.azmeta"), dict(schemaVersion=1, id=ids[str(index)], dependencies=[], settings={}))
                identity = str(uuid.uuid4())
                write(folder / "surface.azurematerial", complete_material_document(material, model, ids))
                write(folder / "surface.azurematerial.azmeta", dict(schemaVersion=1, id=identity, dependencies=list(ids.values()), settings={}))
                level["nodes"][0]["materialSlots"] = {"surface": identity}
            write(folder / "plane.gltf", model)
            write(folder / "level.azurelevel", level)
            command = [str(player.resolve()), "--project", str(project), "--width", "128", "--height", "128", "--capture-frames", "1",
                "--fixed-frame-step", "--qa-camera", "scene", "--qa-light", "scene", "--qa-isolation", "ambient", "--capture-dir", str(folder / "frames")]
            if name == "fixed":
                command += ["--disable-bindless", "--disable-compute-skinning", "--disable-parallel-recording"]
            write(folder / "command.json", command)
            startup = None
            if hasattr(subprocess, "STARTUPINFO"):
                startup = subprocess.STARTUPINFO()
                startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
                startup.wShowWindow = 0
            with (folder / "player.log").open("wb") as log:
                subprocess.run(command, stdout=log, stderr=log, startupinfo=startup, check=True, timeout=240)
            text = (folder / "player.log").read_text(encoding="utf-8", errors="replace")
            assert not re.search(r"VUID-|SYNC-HAZARD|Validation (?:Error|Warning)|\[error\]", text), name
            assert "Allocator after unload: buffers=0 images=0" in text, name
            validation = "Validation layer: enabled" in text
            if expected_validation:
                assert validation == (expected_validation == "enabled"), name
            access = require_texture_access(text, fixed_requested=name == "fixed")
            manifest = json.loads((folder / "frames/capture_manifest.json").read_bytes())
            assert manifest["rasterizationSamples"] == (4 if quality == "multisample" else 1), name
            assert len(manifest["nprRampCompositions"]) == (0 if procedural is None else 1), name
            if procedural is not None:
                assert manifest["nprRampCompositions"][0]["rampComposition"] == npr["rampComposition"], name
            frame = folder / "frames/frame_000000.png"
            image = np.asarray(Image.open(frame).convert("RGB"))
            expected = reference(z, visibility, saturation, procedural, name == "blend")
            actual = linear(image[60:68, 60:68])
            error = float(np.max(np.abs(actual - expected)))
            assert error <= .008, (name, error, expected.tolist(), actual.mean((0, 1)).tolist())
            images[name] = image
            report["captures"].append(dict(name=name, maximumLinearError=error, expected=expected.tolist(), actual=actual.mean((0, 1)).tolist(),
                validationEnabled=validation, textureAccess=access, imageSha256=hashlib.sha256(frame.read_bytes()).hexdigest()))
        for name in ("fixed", "document"):
            if name in images and "saturated" in images:
                assert np.array_equal(images[name], images["saturated"]), name
        assert hashlib.sha256(player.read_bytes()).hexdigest() == report["executableSha256"]
        report["status"] = "passed"
    except Exception as error:
        report.update(status="failed", error=str(error))
        raise
    finally:
        write(output / "summary.json", report)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--case", action="append")
    parser.add_argument("--quality", choices=("existing", "multisample"), default="existing")
    parser.add_argument("--expected-validation", choices=("enabled", "disabled"))
    args = parser.parse_args()
    if args.output:
        run(args.executable, args.output.resolve(), args.case, args.expected_validation, args.quality)
    else:
        with tempfile.TemporaryDirectory(prefix="npr-view-mask-", dir=Path(__file__).resolve().parents[1] / "build/runs") as temporary:
            run(args.executable, Path(temporary) / "capture", args.case, args.expected_validation, args.quality)
