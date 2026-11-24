"""Replay display payload protection, native topmost state, and restart."""
import argparse
import ctypes
import json
import os
from pathlib import Path
from test_original_music_player import fixture as music_fixture, digest, write
from test_notes_player import playing
from test_unlock_room_player import save


def fixture(root, source):
    project = music_fixture(root, source.with_name("music.lua"))
    assets = root / "assets"
    descriptor = json.loads(project.read_bytes())
    descriptor["runtime"]["input"] = {"display-" + key: [] for key in
        ("toggle", "mode", "resolution", "topmost", "topmost-menu", "frame-active", "frame-inactive", "vsync")}
    write(project, descriptor)
    probe = '''
local controllerUpdate = update
local lastProbe
function update()
    controllerUpdate()
    local target = self:find("room-display-window")
    local properties = {"activeFrameRate","inactiveFrameRate","vsync","alwaysOnTop","mode","resolution"}
    local values, changed = {1}, lastProbe == nil
    for i, property in ipairs(properties) do
        values[i+1] = target:get("azure.window-presentation", property)
        if lastProbe and values[i+1] ~= lastProbe[i+1] then changed = true end
    end
    if changed then assert(self:try_write_save("display-probe", values)); lastProbe = values end
end
'''
    (assets / "display.lua").write_text(source.read_text(encoding="utf-8") + probe, encoding="utf-8")
    template = source.with_suffix(".rml").read_text(encoding="utf-8").removeprefix("<style>\n")
    template = template.replace("</style>",
        "#display-toggle { top: 16dp; bottom: auto; } #display-panel { top: 52dp; bottom: auto; }"
        "</style></head><body>")
    (assets / "display.rml").write_text('<rml><head><style>body { font-family: LatoLatin; color: white; }' + template + '</body></rml>', encoding="utf-8")
    level = json.loads((assets / "room.azurelevel").read_bytes())
    component = level["nodes"][0]["components"]
    component.pop("azure.audio-source")
    component["azure.script"]["data"]["asset"] = "assets:/display.lua"
    component["azure.game-ui"]["data"]["asset"] = "assets:/display.rml"
    level["nodes"].append({"id": "room-display-window", "components": {"azure.window-presentation": {
        "type": "azure.window-presentation", "version": 1, "data": {
            "mode": 0, "resolution": 3, "alwaysOnTop": False, "vsync": True,
            "activeFrameRate": 60, "inactiveFrameRate": 24, "screenSleepMode": 1}}}})
    write(assets / "room.azurelevel", level)
    return project


def verify(player, resources, source, output):
    os.environ["VK_INSTANCE_LAYERS"] = "VK_LAYER_KHRONOS_validation"
    os.environ["VK_LOADER_DEBUG"] = "layer"
    output.mkdir()
    project = fixture(output / "fixture", source)
    profile = output / "profile"
    slots = {"room-window-settings": [1, 0, 3, False], "room-frame-rates": [1, 60, 24], "room-vsync": [1, True]}
    for key in slots:
        save(profile, project, key, [2, ["future display settings"]])
    paths = {key: next(profile.glob(f"project-*/save-{key}.json")) for key in slots}
    protected = {key: path.read_bytes() for key, path in paths.items()}
    control = {}

    def ready(window, call):
        control.update(window=window, call=call)

    def topmost():
        window = control["window"]
        window.user.GetWindowLongPtrW.restype = ctypes.c_ssize_t
        return bool(window.user.GetWindowLongPtrW(window.find(), -20) & 8)

    def probe():
        path, = profile.glob("project-*/save-display-probe.json")
        return json.loads(path.read_bytes())["payload"]

    with playing(player, project, resources, profile, output / "protect", on_ready=ready) as (click, press, type_text, screenshot):
        def edit_rate(value):
            click("display-frame-active")
            press(269)
            for _ in range(6):
                press(259)
            type_text(str(value))
            click("display-toggle")
            click("display-toggle")

        click("display-toggle")
        initial = probe()
        click("display-topmost")
        click("display-vsync")
        edit_rate(120)
        assert all(paths[k].read_bytes() == protected[k] for k in slots)
        assert probe() == initial and not topmost(), (probe(), initial)
        screenshot("future-protected.png")
        for key, payload in slots.items():
            save(profile, project, key, payload)
        click("display-topmost")
        click("display-vsync")
        edit_rate(120)
        control["call"]("wait-frames", frames=5)
        assert probe()[1:5] == [120, 24, False, True] and topmost(), probe()
        stored = {key: path.read_bytes() for key, path in paths.items()}
        for path in paths.values():
            path.write_bytes(b'{ "schemaVersion": 1, "payload": [3, ["external future display"]] }')
        external = {key: path.read_bytes() for key, path in paths.items()}
        click("display-topmost")
        click("display-vsync")
        edit_rate(90)
        assert all(paths[k].read_bytes() == external[k] for k in slots)
        assert probe()[1:5] == [120, 24, False, True] and topmost(), probe()
        for key, path in paths.items():
            path.write_bytes(stored[key])
        click("display-topmost")
        click("display-vsync")
        edit_rate(90)
        control["call"]("wait-frames", frames=5)
        assert probe()[1:5] == [90, 24, True, False] and not topmost(), probe()
        screenshot("compatible-restored.png")
    with playing(player, project, resources, profile, output / "reopen", on_ready=ready) as (click, _, _, screenshot):
        click("display-toggle")
        assert probe()[1:5] == [90, 24, True, False] and not topmost(), probe()
        screenshot("reopened.png")
    for stage in ("protect", "reopen"):
        text = (output / stage / "stderr.log").read_text(encoding="utf-8")
        assert 'Insert instance layer "VK_LAYER_KHRONOS_validation"' in text
        assert "VUID-" not in text and "SYNC-HAZARD" not in text
    result = {"playerSHA256": digest(player), "controllerSHA256": digest(source),
              "protectedVersions": [2, 3], "protectedOriginalBytes": True, "reopenedProbe": probe(),
              "preferences": {key: json.loads(path.read_bytes())["payload"] for key, path in paths.items()},
              "nativeTopmostVerified": True, "independentPlayers": 2, "liveAllocationsAfterExit": 0}
    write(output / "verification.json", result)
    print(json.dumps(result))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("player", "resources", "source", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    verify(args.player, args.resources, args.source, args.output)
