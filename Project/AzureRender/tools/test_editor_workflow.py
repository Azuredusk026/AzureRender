"""Exercise production commands in the actual Vulkan editor host."""
import argparse
import json
import pathlib
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", required=True, type=pathlib.Path)
    parser.add_argument("--project", required=True, type=pathlib.Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="azure_editor_") as folder:
        root = pathlib.Path(folder)
        shutil.copytree(args.project.resolve().parent, root / "game")
        good_script=(root / "game/assets/player.lua").read_text(encoding="utf-8").replace("local autoplay = false","local autoplay = true")
        (root / "game/assets/player.lua").write_text(good_script,encoding="utf-8")
        # Nine running frames must yield nine fixed steps, independent of views.
        cadence_task = root / "cadence-actions.json"
        cadence_task.write_text(json.dumps([
            {"frame": 2, "command": "play"},
            {"frame": 11, "command": "pause"},
            {"frame": 12, "command": "stop"},
        ]), encoding="utf-8")
        cadence_report = root / "cadence-report.json"
        cadence = subprocess.run([str(args.executable.resolve()), "--editor-project", str(root / "game/project.azureproject"),
            "--editor-actions", str(cadence_task), "--runtime-report", str(cadence_report),
            "--fixed-frame-step", "--smoke-frames", "13"], cwd=root, capture_output=True,
            text=True, encoding="utf-8", errors="replace", timeout=90)
        assert cadence.returncode == 0, cadence.stdout + cadence.stderr
        cadence_data = json.loads(cadence_report.read_text(encoding="utf-8"))
        assert cadence_data["editorPlaySteps"] == 9, cadence_data
        actions = [
            {"frame": 1, "command": "import", "path": str(args.project.resolve().parent.parent / "test_model.gltf")},
            {"frame": 2, "command": "place"},
            {"frame": 3, "command": "rename", "value": "Workflow Object"},
            {"frame": 4, "command": "duplicate"},
            {"frame": 5, "command": "select", "indices": [3, 4]},
            {"frame": 6, "command": "delete"},
            {"frame": 7, "command": "undo"},
            {"frame": 8, "command": "save"},
            {"frame": 8, "command": "select", "indices": [1]},
            {"frame": 8, "command": "component-add", "type": "azure.third-person-camera"},
            {"frame": 8, "command": "component-field", "type": "azure.third-person-camera", "field": "target", "value": "hero"},
            {"frame": 8, "command": "component-field", "type": "azure.animator", "field": "state", "value": "run"},
            {"frame": 8, "command": "preview", "state": "run", "time": 0.25},
            {"frame": 8, "command": "clear-preview"},
            {"frame": 8, "command": "save"},
            {"frame": 8, "command": "reload"},
            {"frame": 9, "command": "play"},
            {"frame": 11, "command": "write-script", "path": "assets:/player.lua", "value": "function update(dt) error('workflow script fault') end"},
            {"frame": 13, "command": "write-script", "path": "assets:/player.lua", "value": good_script},
            {"frame": 15, "command": "pause"},
            {"frame": 17, "command": "step"},
            {"frame": 19, "command": "resume"},
            {"frame": 22, "command": "level", "value": "assets:/destination.azurelevel"},
            {"frame": 23, "command": "wait-level", "value": "assets:/destination.azurelevel"},
            {"frame": 27, "command": "stop"},
        ]
        task = root / "actions.json"
        task.write_text(json.dumps(actions), encoding="utf-8")
        report = root / "report.json"
        process = subprocess.run([str(args.executable.resolve()), "--editor-project", str(root / "game/project.azureproject"),
            "--editor-actions", str(task), "--runtime-report", str(report), "--fixed-frame-step", "--smoke-frames", "32"],
            cwd=root, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=90)
        if process.returncode:
            raise AssertionError(process.stdout + process.stderr)
        data = json.loads(report.read_text(encoding="utf-8"))
        assert len(data["editorActions"]) == len(actions), data
        assert all(action["passed"] for action in data["editorActions"]), data
        assert data["editorPlaySteps"] >= 10 and data["editorLevelRevision"] >= 2, data
        assert data["uiDrawCalls"] > 0 and data["animationFrames"] > 0, data
        assert data["observedScriptErrors"] >= 1 and data["recoveredScripts"] >= 2, data
        assert data["editStateRestored"] and not data["editorPlaying"], data
        assert data["presentationErrors"] == [], data
        assert "Validation Error" not in process.stdout + process.stderr, process.stdout + process.stderr
        reopened = json.loads((root / "game/assets/courtyard.azurelevel").read_text(encoding="utf-8"))
        assert len(reopened["nodes"]) == 5, reopened
        hero = next(node for node in reopened["nodes"] if node["id"] == "hero")
        assert hero["components"]["azure.third-person-camera"]["data"]["target"] == "hero", hero
        assert hero["components"]["azure.animator"]["data"]["state"] == "run", hero
        print(json.dumps(data, ensure_ascii=False))


if __name__ == "__main__":
    main()
