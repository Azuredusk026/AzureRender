"""Exercise authored material classes and effect contributions on Vulkan."""
import argparse
import json
from pathlib import Path
import tempfile
import base64
import struct
import re
import numpy as np
from test_character_finish import capture, pixels, digest


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--asset", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="azure_material_fixture_") as directory:
        root = args.output.resolve() if args.output else Path(directory)
        root.mkdir(parents=True, exist_ok=True)
        executable, asset = args.executable.resolve(), args.asset.resolve()
        scene = Path(__file__).resolve().parents[1] / "assets_public/scenes/material_mixed.azscene"
        document = json.loads(asset.read_text())
        body = document["meshes"][0]["primitives"][0]["attributes"]["POSITION"]
        floor_y = float(re.search(r'node "floor"[^\n]*\nvisible true\ntransform 0 ([^ ]+)', scene.read_text())[1])
        # Public box has a bind-space maximum Y of 1, scaled to 0.1 in this scene.
        assert abs(document["accessors"][body]["min"][1] - (floor_y + .1)) <= .001, "Fixture is not grounded"
        results = {}
        for name, view, extra in [("beauty", "beauty", ()), ("classes", "material-id", ()),
                ("brow", "brow-mask", ()), ("hair", "hair-kk", ()), ("face", "face-sdf", ()),
                ("overlay-off", "beauty", ("--qa-effect", "overlay", "--qa-effect-state", "disabled"))]:
            output = capture(executable, asset, root, name, "full-body-front", "specular-rim", view, extra=extra)
            results[name] = pixels(output / "frame_000000.png")
        manifest = json.loads((root / "beauty/capture_manifest.json").read_text())
        classes = {m["class"] for m in manifest["materialInventory"] if m["primitiveCount"]}
        assert {"generic", "face", "hair", "overlay"} <= classes, classes
        brow = results["brow"].min(axis=2) > 200
        assert brow.sum() >= 20, int(brow.sum())
        delta = np.abs(results["beauty"] - results["overlay-off"]).max(axis=2)
        assert np.mean(delta[brow] > 1) >= .9
        # Ignore the background, which QA keeps on the normal compositing path.
        classified = results["classes"]
        hair_region = (classified[:,:,0] > classified[:,:,1] * 1.3) & (classified[:,:,2] > classified[:,:,1] * 1.1)
        face_region = (classified.min(axis=2) > 200) & (classified[:,:,0] > classified[:,:,1]) & (classified[:,:,1] > classified[:,:,2])
        assert hair_region.sum() > 100 and face_region.sum() > 100
        hair_lit = int(np.count_nonzero(results["hair"].max(axis=2)[hair_region] > 20))
        face_lit = int(np.count_nonzero(results["face"].max(axis=2)[face_region] > 20))
        assert hair_lit > 20, hair_lit
        assert face_lit > 100, face_lit
        report = {"assetSha256": digest(asset), "classes": sorted(classes), "browPixels": int(brow.sum()),
                  "overlayContribution": float(np.mean(delta[brow] > 1)), "hairHighlightPixels": hair_lit,
                  "faceSdfPixels": face_lit, "validationErrors": 0}
        mixed = capture(executable, asset, root, "mixed", "full-body-front", "specular-rim",
                        extra=("--scene", str(scene)))
        mixed_difference = np.abs(results["beauty"] - pixels(mixed / "frame_000000.png")).max(axis=2)
        assert json.loads((mixed / "capture_manifest.json").read_text())["sceneWorldCoordinates"]
        report["mixedSceneChangedPixels"] = int(np.count_nonzero(mixed_difference > 3))
        assert report["mixedSceneChangedPixels"] > 10000, report
        # Author a short head motion on the public rig; brow is a child joint.
        animated = json.loads(asset.read_text())
        for image in animated["images"]:
            data = (asset.parent / image["uri"]).read_bytes()
            image["uri"] = "data:image/png;base64," + base64.b64encode(data).decode()
        binary = struct.pack("<3f12f", 0, .1, .2, 0, 0, 0, 1,
                             0, .0749297, 0, .9971888, 0, 0, 0, 1)
        buffer = len(animated["buffers"])
        animated["buffers"].append({"byteLength": len(binary), "uri": "data:application/octet-stream;base64," + base64.b64encode(binary).decode()})
        view = len(animated["bufferViews"])
        animated["bufferViews"].extend([{"buffer": buffer, "byteOffset": 0, "byteLength": 12},
                                        {"buffer": buffer, "byteOffset": 12, "byteLength": 48}])
        key = len(animated["accessors"])
        animated["accessors"].extend([{"bufferView": view, "componentType": 5126, "count": 3, "type": "SCALAR", "min": [0], "max": [.2]},
                                      {"bufferView": view + 1, "componentType": 5126, "count": 3, "type": "VEC4"}])
        animated["animations"] = [{"name": "head-follow-probe", "samplers": [{"input": key, "output": key + 1, "interpolation": "LINEAR"}],
                                   "channels": [{"sampler": 0, "target": {"node": 1, "path": "rotation"}}]}]
        animation_asset = root / "head-follow.gltf"
        animation_asset.write_text(json.dumps(animated), encoding="utf-8")
        moving = capture(executable, animation_asset, root, "head-mask", "full-body-front", "specular-rim", "brow-mask", frames=12,
                         extra=("--qa-animation",))
        moving_cpu = capture(executable, animation_asset, root, "head-mask-cpu", "full-body-front", "specular-rim", "brow-mask", frames=12,
                             extra=("--disable-compute-skinning", "--qa-animation"))
        centers = []
        for frame in sorted(moving.glob("frame_*.png")):
            image = pixels(frame)
            region = image.min(axis=2) > 200
            assert region.sum() > 20, frame.name
            centers.append(float(np.nonzero(region)[1].mean()))
            cpu_image = pixels(moving_cpu / frame.name)
            assert np.abs(cpu_image - image).mean() < .05, frame.name
        report["headFollowMotionPixels"] = max(centers) - min(centers)
        assert report["headFollowMotionPixels"] > 2, centers
        (root / "summary.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
        print(json.dumps(report))


if __name__ == "__main__":
    main()
