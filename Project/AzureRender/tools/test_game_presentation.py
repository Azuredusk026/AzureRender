"""Verify UI compositing and state-driven skinning on the real Vulkan backend."""
import argparse
import json
import pathlib
import shutil
import subprocess
import tempfile
import numpy as np
from PIL import Image


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", required=True, type=pathlib.Path)
    parser.add_argument("--project", required=True, type=pathlib.Path)
    parser.add_argument("--output", type=pathlib.Path)
    args = parser.parse_args()
    results = {}
    with tempfile.TemporaryDirectory(prefix="azure_presentation_") as directory:
        root = pathlib.Path(directory)
        for name, state, ui in [("idle", "idle", True), ("run", "run", True), ("no-ui", "idle", False)]:
            game = root / name
            shutil.copytree(args.project.resolve().parent, game)
            level_path = game / "assets/courtyard.azurelevel"
            level = json.loads(level_path.read_text(encoding="utf-8"))
            for node in level["nodes"]:
                node["components"].pop("azure.script", None)
                node["components"].pop("azure.character", None)
                if node["id"] == "hero":
                    node["components"]["azure.animator"]["data"]["state"] = state
                    node["components"]["azure.game-ui"]["data"]["enabled"] = ui
            level_path.write_text(json.dumps(level), encoding="utf-8")
            capture = root / (name + "-capture")
            report = root / (name + "-report.json")
            result = subprocess.run([str(args.executable.resolve()), "--project", str(game / "project.azureproject"),
                "--capture-dir", str(capture), "--capture-frames", "2", "--capture-fps", "4", "--runtime-report", str(report)],
                cwd=root, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=60)
            assert result.returncode == 0, result.stdout + result.stderr
            assert "VUID-" not in result.stderr and "Validation Error" not in result.stderr, result.stderr
            assert "Allocator after unload: buffers=0 images=0" in result.stdout, result.stdout
            image = sorted(capture.glob("*.png"))[-1]
            results[name] = np.asarray(Image.open(image).convert("RGB"), dtype=np.int16)
            data = json.loads(report.read_text())
            assert data["animationFrames"] > 0 and not data["presentationErrors"], data
            assert (data["uiDrawCalls"] > 0) == ui, data
            if args.output:
                args.output.mkdir(parents=True, exist_ok=True)
                shutil.copy2(image, args.output / (name + ".png"))
                (args.output / (name + ".json")).write_text(json.dumps(data, indent=2), encoding="utf-8")
        animation_difference = np.abs(results["idle"] - results["run"])
        # Exclude the HUD: the remaining image must reflect a different sampled clip.
        animation_difference[:250, :500] = 0
        ui_difference = np.abs(results["idle"] - results["no-ui"])
        changed_animation = int(np.count_nonzero(np.max(animation_difference, axis=2) > 3))
        changed_ui = int(np.count_nonzero(np.max(ui_difference[:250, :500], axis=2) > 3))
        assert changed_animation > 100, changed_animation
        assert changed_ui > 1000, changed_ui
        evidence = {"animationChangedPixels": changed_animation, "uiChangedPixels": changed_ui}
        print(json.dumps(evidence))
        if args.output:
            (args.output / "summary.json").write_text(json.dumps(evidence, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
