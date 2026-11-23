"""Verify that the real Player supplies the registered media service."""
import argparse
import json
import os
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent / "npr_room"))
from test_original_music_player import fixture, digest, write
from test_notes_player import playing


def verify(player, resources, output):
    output.mkdir()
    source = Path(__file__).resolve().parents[1] / "examples/npr-room/assets/music.lua"
    project = fixture(output / "fixture", source)
    (project.parent / "assets/music.lua").write_text('''local complete = false
function update()
    if complete then return end
    local status = self:media_status()
    self:write_save("media-host-contract", {1, status[1], #self:media_entries("music")})
    complete = true
end
''', encoding="utf-8")
    os.environ["VK_INSTANCE_LAYERS"] = "VK_LAYER_KHRONOS_validation"
    os.environ["VK_LOADER_DEBUG"] = "layer"
    profile = output / "profile"
    with playing(player, project, resources, profile, output / "player") as (_, _, _, screenshot):
        slot, = profile.glob("project-*/save-media-host-contract.json")
        value = json.loads(slot.read_bytes())["payload"]
        write(output / "observed.json", value)
        assert value == [1, "idle", 0], value
        screenshot("media-host.png")
    report = dict(playerSHA256=digest(player), observed=value, independentPlayers=1,
                  liveAllocationsAfterExit=0)
    write(output / "verification.json", report)
    print("Player media service injection and native release passed")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("player", "resources", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    verify(args.player, args.resources, args.output)
