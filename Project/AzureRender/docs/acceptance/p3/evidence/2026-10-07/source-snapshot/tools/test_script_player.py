"""Exercise the shipped two-level Lua gameplay sample in a real Vulkan Player."""
import json
import pathlib
import shutil
import subprocess
import sys
import tempfile

exe = pathlib.Path(sys.argv[1]).resolve()
sample = pathlib.Path(sys.argv[2]).resolve()
with tempfile.TemporaryDirectory() as directory:
    root = pathlib.Path(directory)
    project = root / "game"
    shutil.copytree(sample, project)
    player = project / "assets/player.lua"
    player.write_text(player.read_text().replace("local autoplay = false", "local autoplay = true"))
    (project / "assets/fault.lua").write_text("function update(dt) error('isolated player failure') end")
    for name in ["courtyard", "destination"]:
        path = project / f"assets/{name}.azurelevel"
        level = json.loads(path.read_text())
        level["nodes"].append({"id": "fault", "components": {"azure.script": {
            "type": "azure.script", "version": 1, "data": {"asset": "assets:/fault.lua"}}}})
        path.write_text(json.dumps(level))
    report = root / "report.json"
    result = subprocess.run([
        str(exe), "--project", str(project / "project.azureproject"),
        "--smoke-frames", "180", "--fixed-frame-step", "--runtime-report", str(report)
    ], cwd=root, capture_output=True, text=True, timeout=90)
    assert result.returncode == 0, result.stdout + result.stderr
    data = json.loads(report.read_text())
    hero = next(node for node in data["nodes"] if node["id"] == "hero")
    assert data["scene"] == "destination" and data["levelRevision"] == 2, data
    assert data["fixedSteps"] == 179 and hero["translation"][0] > 5 and 0.8 < hero["translation"][1] < 1.2, data
    assert data["activeScripts"] == 1 and any("isolated player failure" in error for error in data["scriptErrors"]), data
    assert "VUID-" not in result.stderr and "Allocator after unload: buffers=0 images=0" in result.stdout
    print(json.dumps(data))
    print("Vulkan Player Lua motion, trigger, level switch, continued play and error isolation passed")
