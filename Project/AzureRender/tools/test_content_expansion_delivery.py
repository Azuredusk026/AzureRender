"""Render two 2D projects after movable Release publication."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import build_game


def run(command, cwd, env=None):
    result = subprocess.run(list(map(str, command)), cwd=cwd, env=env, capture_output=True,
                            encoding="utf-8", errors="replace", timeout=180)
    (cwd / "player.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    assert result.returncode == 0, result.stdout + result.stderr
    assert "VUID-" not in result.stderr and "Validation Error" not in result.stderr
    return result.stdout


def main(build, output):
    output.mkdir(parents=True, exist_ok=False)
    source = Path(__file__).resolve().parents[1]
    install = output / "engine"
    run(["cmake", "--install", build, "--config", "Release", "--prefix", install], output)
    results = []
    for index, kind in enumerate(("sprite", "rectangle")):
        project = output / f"project-{kind}"
        shutil.copytree(source / "assets_public/scene_inspector", project, ignore=shutil.ignore_patterns(".azure", "*.azmeta"))
        config = json.loads((project / "project.azureproject").read_text(encoding="utf-8"))
        scene_path = project / "assets/inspection.azurelevel"
        scene = json.loads(scene_path.read_text(encoding="utf-8"))
        scene["nodes"].append({"id": "hud", "resourceId": "", "components": {
            "azure.transform": {"type": "azure.transform", "version": 1, "data": {}},
            "azure.game-ui": {"type": "azure.game-ui", "version": 1, "data": {"asset": "assets:/scene.azure2d"}}}})
        scene_path.write_text(json.dumps(scene), encoding="utf-8")
        image = "<svg width='16' height='16'><rect width='16' height='16' fill='#ff0000'/></svg>"
        (project / "assets/精灵.svg").write_text(image, encoding="utf-8")
        item = {"id": "tile", "kind": kind, "rect": [10, 10, 100, 100], "color": "#ff0000"}
        if kind == "sprite": item["image"] = "精灵.svg"
        content = {"schemaVersion": 1, "size": [640, 360], "items": [item,
            {"id": "label", "kind": "text", "rect": [10, 125, 500, 60],
             "text": "二维场景：中文标签与资源移动", "fontSize": 26, "color": "#ffffff"}]}
        (project / "assets/scene.azure2d").write_text(json.dumps(content, ensure_ascii=False), encoding="utf-8")
        package = output / f"package-{kind}"
        build_game.build_game(project / "project.azureproject", install, package)
        moved = output / f"Moved {kind} game"
        package.rename(moved)
        project.rename(output / f"source-unavailable-{index}")
        env = {key: value for key, value in os.environ.items() if not key.upper().startswith("AZURERENDER_")}
        env["PATH"] = str(Path(env["SYSTEMROOT"]) / "System32")
        capture = output / f"{kind}-capture"
        unavailable = output / "engine-unavailable"
        install.rename(unavailable)
        try:
            log = run([moved / "bin/AzurePlayer.exe", "--project", moved / "game/project.azureproject",
                "--resource-root", moved / "share/AzureRender", "--width", "640", "--height", "360",
                "--capture-dir", capture, "--capture-frames", "8", "--fixed-frame-step",
                "--runtime-report", moved / "runtime.json"], moved, env)
        finally:
            unavailable.rename(install)
        report = json.loads((moved / "runtime.json").read_text(encoding="utf-8"))
        assert report["uiDrawCalls"] > 0 and not report["scriptErrors"] and not report["presentationErrors"], report
        assert "Allocator after unload: buffers=0 images=0" in log
        # Visible pixels prove the imported texture and geometry reach Vulkan.
        from PIL import Image
        screenshot = sorted(capture.glob("frame_*.png"))[-1]
        with Image.open(screenshot) as frame:
            red = frame.convert("RGB").getpixel((45, 45))
            assert red[0] > 180 and red[1] < 90 and red[2] < 90, red
        (moved / "runtime.json").rename(output / f"{kind}-runtime.json")
        (moved / "player.log").rename(output / f"{kind}-player.log")
        manifest = build_game.verify_package(moved)
        assert any(file["path"].endswith("NanoSVG-source.json") for file in manifest["files"])
        assert any(file["path"].endswith("NotoSansCJKsc-Regular.otf") for file in manifest["files"])
        results.append({"workflow": kind, "uiDrawCalls": report["uiDrawCalls"], "screenshot": str(screenshot), "files": len(manifest["files"])})
    summary = {"passed": True, "workflows": results, "physicalInput": "pending", "fontAppearance": "pending human review"}
    (output / "summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(summary, ensure_ascii=False))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    main(args.build_dir.resolve(), args.output.resolve())
