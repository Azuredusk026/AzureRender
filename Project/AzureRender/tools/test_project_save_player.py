"""Verify project save compatibility in the real exploration Player."""
import argparse
import json
import os
from pathlib import Path
import shutil
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent / "npr_room"))
from test_notes_player import playing
from test_original_music_player import digest, write


def verify(player, resources, template, output):
    output.mkdir()
    fixture = output / "fixture"
    shutil.copytree(template, fixture, ignore=shutil.ignore_patterns(".azure", "*.azmeta", "*.tmp"))
    project = fixture / "project.azureproject"
    descriptor = json.loads(project.read_bytes())
    level = fixture / "assets" / descriptor["startupScene"].split(":/", 1)[1]
    document = json.loads(level.read_bytes())
    assert not any(node["id"] == "save-contract-probe" for node in document["nodes"])
    document["nodes"].append({"id": "save-contract-probe", "components": {
        "azure.script": {"type": "azure.script", "version": 1,
                         "data": {"asset": "assets:/save-contract.lua"}}}})
    write(level, document)
    (fixture / "assets/save-contract.lua").write_text('''local complete = false
function update()
    if complete then return end
    local value = self:read_save("storage-probe")
    local total = type(value) == "table" and value[1] == 1 and value[2] or 0
    self:write_save("storage-probe", {1, total + 1})
    complete = true
end
''', encoding="utf-8")
    os.environ["VK_INSTANCE_LAYERS"] = "VK_LAYER_KHRONOS_validation"
    os.environ["VK_LOADER_DEBUG"] = "layer"
    profile = output / "profile"
    states = []
    for index in (1, 2):
        stage = output / f"player-{index}"
        with playing(player, project, resources, profile, stage,
                     level_reference=descriptor["startupScene"]) as (_, _, _, screenshot):
            slot, = profile.glob("project-*/save-storage-probe.json")
            value = json.loads(slot.read_bytes())["payload"]
            assert value == [1, index], (index, value)
            states.append(value)
            screenshot("exploration.png")
        log = (stage / "stderr.log").read_text(encoding="utf-8")
        assert 'Insert instance layer "VK_LAYER_KHRONOS_validation"' in log
        assert "VUID-" not in log and "SYNC-HAZARD" not in log
    report = {"playerSHA256": digest(player), "states": states, "independentPlayers": 2,
              "liveAllocationsAfterExit": 0,
              "fixtureSHA256": {path.relative_to(fixture).as_posix(): digest(path)
                                for path in sorted(fixture.rglob("*"))
                                if path.is_file() and ".azure" not in path.parts}}
    write(output / "verification.json", report)
    print("Exploration Player save, reload and native release passed")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("player", "resources", "template", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    verify(args.player, args.resources, args.template, args.output)
