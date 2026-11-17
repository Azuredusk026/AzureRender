"""Replay offline achievement recovery in independent Players with a local backend."""

import argparse
import json
from pathlib import Path
import subprocess

from tools.npr_room.build_story_room import EXAMPLE
from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest


CATEGORY = "COMPLETE_TODO_COUNT"
SCRIPT = """
local category = "COMPLETE_TODO_COUNT"
local ran = false
function update()
    if ran then return end
    ran = true
    local self_host = self
    local backend, staged, writes, stores = {}, nil, 0, 0
    function backend:initialized() return phase ~= 1 end
    function backend:get_stat(key)
        if phase == 1 then return nil end
        if key == "POMODORO_WORK_HOURS_COUNT" then return 0 end
        assert(key == category)
        local persisted = self_host:read_save("achievement-fixture-backend")
        return persisted == nil and 0 or persisted[2]
    end
    function backend:set_stat(key, value)
        assert(key == category)
        staged, writes = value, writes + 1
        return true
    end
    function backend:store_stats()
        stores = stores + 1
        if phase == 2 then return false end
        return self_host:try_write_save("achievement-fixture-backend", {1, staged})
    end
    local stats = new_platform_achievement_stats(backend)
    local room = new_room_achievements(self, stats)
    if phase == 1 then
        assert(room:once(category) and room:once(category))
    else
        room:restore(false, {}, 0, 0)
    end
    self:write_save("achievement-fixture-report", {1, phase, writes, stores, stats:progress(category) or -1})
end
"""


def replay(player, resources, output):
    output.mkdir()
    assets = output / "assets"
    assets.mkdir()
    profile = output / "profile"
    project = output / "fixture.azureproject"
    write(project, {"schemaVersion": 1, "id": "npr-local-achievement-fixture", "name": "Local achievement fixture",
                    "mounts": [{"name": "assets", "path": "assets"}], "startupScene": "assets:/fixture.azurelevel"})
    level = json.loads((EXAMPLE / "boot.azurelevel").read_bytes())
    level["nodes"][0]["components"]["azure.script"]["data"]["asset"] = "assets:/fixture.lua"
    write(assets / "fixture.azurelevel", level)
    logic = (EXAMPLE / "room-achievements.lua").read_text(encoding="utf-8")

    def saved(slot):
        paths = list(profile.glob(f"project-*/save-{slot}.json"))
        if not paths:
            return None
        path, = paths
        data = json.loads(path.read_bytes())
        assert data["schemaVersion"] == 1
        return data["payload"]

    reports = []
    for phase, expected in enumerate(([0, 0, -1], [1, 1, 1], [1, 1, 1], [0, 0, 1]), 1):
        (assets / "fixture.lua").write_text(f"local phase = {phase}\n{logic}\n{SCRIPT}", encoding="utf-8")
        runtime = output / f"runtime-{phase}.json"
        run = subprocess.run([str(player), "--project", str(project), "--resource-root", str(resources),
                              "--save-root", str(profile), "--runtime-report", str(runtime), "--smoke-frames", "30",
                              "--width", "640", "--height", "360"], cwd=output,
                             capture_output=True, encoding="utf-8", errors="replace", timeout=80)
        (output / f"stdout-{phase}.log").write_text(run.stdout, encoding="utf-8")
        (output / f"stderr-{phase}.log").write_text(run.stderr, encoding="utf-8")
        assert run.returncode == 0 and "Allocator after unload: buffers=0 images=0" in run.stdout, run.stderr[-3500:]
        result = json.loads(runtime.read_bytes())
        assert not any(result.get(key) for key in ("scriptErrors", "presentationErrors", "loadError")), result
        assert result["activeScripts"] == 1
        report = saved("achievement-fixture-report")
        assert report == [1, phase, *expected], report
        assert saved("room-achievements") == [1, [CATEGORY]]
        assert saved("achievement-fixture-backend") == ([1, 1] if phase >= 3 else None)
        reports.append({"launch": phase, "statWrites": expected[0], "storeAttempts": expected[1],
                        "seenProgress": expected[2], "backendPersisted": phase >= 3})
    report = {"playerSHA256": digest(player), "logicSHA256": digest(EXAMPLE / "room-achievements.lua"),
              "fixture": "local", "launches": reports, "category": CATEGORY, "cleanAllocatorExits": 4}
    write(output / "achievement-platform-replay.json", report)
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("player", "resource-root", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(replay(args.player.resolve(), args.resource_root.resolve(), args.output.resolve())))
