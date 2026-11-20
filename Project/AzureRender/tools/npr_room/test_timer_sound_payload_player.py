"""Replay timer sound payload protection and same-session recovery."""
import argparse
import json
import os
from pathlib import Path
from test_timer_preview_recovery_player import fixture
from test_original_music_player import digest, write
from test_notes_player import playing
from test_unlock_room_player import save


def verify(player, resources, source, output):
    os.environ["VK_INSTANCE_LAYERS"] = "VK_LAYER_KHRONOS_validation"
    os.environ["VK_LOADER_DEBUG"] = "layer"
    output.mkdir()
    project = fixture(output / "fixture", source)
    profile = output / "profile"
    name = "timer-sound-settings"
    save(profile, project, name, [2, ["future timer sound preferences"]])
    slot, = profile.glob("project-*/save-timer-sound-settings.json")
    protected = slot.read_bytes()
    defaults = [1, 50, 0, 1, 50, 0]
    with playing(player, project, resources, profile, output / "protect") as (click, _, _, screenshot):
        click("timer-sound-toggle")
        click("timer-sound-start-2")
        assert slot.read_bytes() == protected
        screenshot("future-protected.png")
        save(profile, project, name, [1, defaults, defaults])
        click("timer-sound-start-2")
        expected = [1, [2, 50, 0, 1, 50, 0], defaults]
        assert json.loads(slot.read_bytes())["payload"] == expected
        stored = slot.read_bytes()
        external = b'{ "schemaVersion": 1, "payload": [3, ["external future value"]] }'
        slot.write_bytes(external)
        click("timer-sound-start-3")
        assert slot.read_bytes() == external
        slot.write_bytes(stored)
        click("timer-sound-start-3")
        expected = [1, [3, 50, 0, 1, 50, 0], defaults]
        assert json.loads(slot.read_bytes())["payload"] == expected
        screenshot("compatible-restored.png")
    with playing(player, project, resources, profile, output / "reopen", 2) as (click, _, _, screenshot):
        click("timer-sound-toggle")
        click("timer-sound-start-3")
        assert json.loads(slot.read_bytes())["payload"] == expected
        screenshot("reopened.png")
    for folder in ("protect", "reopen"):
        text = (output / folder / "stderr.log").read_text(encoding="utf-8")
        assert 'Insert instance layer "VK_LAYER_KHRONOS_validation"' in text
        assert "VUID-" not in text and "SYNC-HAZARD" not in text
    result = {"playerSHA256": digest(player), "controllerSHA256": digest(source / "timer-sound-settings.lua"),
              "protectedPayloadVersions": [2, 3], "protectedOriginalBytes": True,
              "sameSessionRecovery": expected, "independentPlayers": 2, "liveAllocationsAfterExit": 0}
    write(output / "verification.json", result)
    print(json.dumps(result))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("player", "resources", "source", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    verify(args.player, args.resources, args.source, args.output)
