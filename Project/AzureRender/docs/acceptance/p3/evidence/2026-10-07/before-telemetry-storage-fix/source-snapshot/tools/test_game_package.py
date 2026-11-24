"""Publish through the Vulkan editor, move the game and run with an isolated PATH."""
import argparse
import json
import os
import pathlib
import shutil
import subprocess
import tempfile
import build_game


def run(command, cwd, env=None, success=True, timeout=180):
    result = subprocess.run(list(map(str, command)), cwd=cwd, env=env, capture_output=True,
        text=True, encoding="utf-8", errors="replace", timeout=timeout)
    assert (result.returncode == 0) == success, result.stdout + result.stderr
    return result.stdout + result.stderr


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", required=True, type=pathlib.Path)
    parser.add_argument("--editor", required=True, type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    parser.add_argument("--evidence", required=True, type=pathlib.Path)
    args = parser.parse_args()
    output, evidence = args.output.resolve(), args.evidence.resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="azure_game_publish_") as temporary:
        root = pathlib.Path(temporary)
        engine, project = root / "Release engine", root / "New game"
        run(["cmake", "--install", args.build_dir.resolve(), "--config", "Release", "--prefix", engine], root)
        player = engine / "bin/AzurePlayer.exe"
        run([player, "--create-game", project], root)
        script = project / "assets/player.lua"
        script.write_text(script.read_text(encoding="utf-8").replace("local autoplay = false", "local autoplay = true"), encoding="utf-8")
        # The same output can be reused by later regression runs.
        actions = [{"frame": 1, "command": "build", "install": str(engine), "output": str(output), "replace": output.exists()},
                   {"frame": 2, "command": "wait-build"}]
        task, report = root / "actions.json", evidence / "editor-report.json"
        task.write_text(json.dumps(actions), encoding="utf-8")
        log = run([args.editor.resolve(), "--editor-project", project / "project.azureproject", "--editor-actions", task,
                   "--smoke-frames", "8", "--fixed-frame-step", "--runtime-report", report], root)
        (evidence / "editor.log").write_text(log, encoding="utf-8")
        editor_report = json.loads(report.read_text(encoding="utf-8"))
        assert len(editor_report["editorActions"]) == 2 and all(item["passed"] for item in editor_report["editorActions"]), editor_report
        assert "VUID-" not in log and "Validation Error" not in log and "Allocator after unload: buffers=0 images=0" in log, log
        manifest = build_game.verify_package(output)
        assert all("AzureRender.exe" not in item["path"] and "AzureMetaGen.exe" not in item["path"] for item in manifest["files"])
        assert not (output / "bin/rmlui_debugger.dll").exists()
        assert (output / "bin/msvcp140.dll").is_file() and (output / "bin/vcruntime140.dll").is_file()
        assert all(not any(name in pathlib.PurePosixPath(item["path"]).parts for name in build_game.PRIVATE_NAMES | build_game.GENERATED_NAMES) for item in manifest["files"])
        original_manifest = (output / "game_manifest.json").read_bytes()
        # A real broken dependency must fail before replacing the valid package.
        sound = project / "assets/portal.wav"
        sound.rename(sound.with_suffix(".wav.hidden"))
        failure = run(["python", engine / "share/AzureRender/tools/build_game.py", "--project", project / "project.azureproject",
                       "--install", engine, "--output", output, "--replace"], root, success=False)
        assert "dependency" in failure.lower(), failure
        assert (output / "game_manifest.json").read_bytes() == original_manifest
        sound.with_suffix(".wav.hidden").rename(sound)
        (evidence / "failure-rollback.log").write_text(failure, encoding="utf-8")
        env = {key.upper(): value for key, value in os.environ.items() if not key.upper().startswith("AZURERENDER_")}
        env["PATH"] = str(pathlib.Path(env["SYSTEMROOT"]) / "System32")
        moved = root / "Moved game with spaces"
        shutil.move(output, moved)
        project.rename(root / "Source unavailable")
        engine.rename(root / "Engine unavailable")
        try:
            report = evidence / "player-report.json"
            log = run([env["COMSPEC"], "/d", "/c", "start-game.cmd", "--smoke-frames", "180", "--fixed-frame-step",
                       "--runtime-report", report], moved, env)
            (evidence / "player.log").write_text(log, encoding="utf-8")
            data = json.loads(report.read_text(encoding="utf-8"))
            hero = next(node for node in data["nodes"] if node["id"] == "hero")
            assert data["scene"] == "destination" and data["levelRevision"] == 2 and data["fixedSteps"] == 179, data
            assert hero["translation"][0] > 5 and 0.8 < hero["translation"][1] < 1.2, data
            assert data["audioStarts"] > 0 and data["uiDrawCalls"] > 0 and data["animationFrames"] > 0, data
            assert not data["scriptErrors"] and not data["presentationErrors"], data
            assert "VUID-" not in log and "Validation Error" not in log and "Allocator after unload: buffers=0 images=0" in log, log
            build_game.verify_package(moved)
            immutable = moved / "game/assets/player.lua"
            saved = immutable.read_bytes(); immutable.write_bytes(saved + b"\n-- tamper\n")
            try:
                build_game.verify_package(moved)
                raise AssertionError("Tamper was accepted")
            except ValueError:
                pass
            finally:
                immutable.write_bytes(saved)
            shutil.rmtree(moved / "captures", ignore_errors=True)
            shutil.rmtree(moved / "game/.azure", ignore_errors=True)
        finally:
            shutil.move(moved, output)
        build_game.verify_package(output)
        summary = {"status": "passed", "package": str(output), "files": len(manifest["files"]),
            "isolatedPath": env["PATH"], "movedDirectory": True, "sourceUnavailable": True, "engineUnavailable": True,
            "dependencyFailureRollback": True, "tamperRejected": True, "scene": data["scene"],
            "fixedSteps": data["fixedSteps"], "hero": hero["translation"], "audioStarts": data["audioStarts"],
            "uiDrawCalls": data["uiDrawCalls"], "animationFrames": data["animationFrames"]}
        (evidence / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(summary))


if __name__ == "__main__":
    main()
