"""Replay failed conversation checkpoint recovery and newer payload protection."""
import argparse
import json
import os
from pathlib import Path
from test_optional_voice_player import fixture
from test_original_music_player import digest, write
from test_notes_player import playing
from test_unlock_room_player import save


def verify(player, resources, source, output):
    os.environ["VK_INSTANCE_LAYERS"] = "VK_LAYER_KHRONOS_validation"
    os.environ["VK_LOADER_DEBUG"] = "layer"
    output.mkdir()
    project = fixture(output / "fixture", source)
    profile, control = output / "profile", {}
    save(profile, project, "work-conversation", [1,0,0,0])
    slot, = profile.glob("project-*/save-work-conversation.json")
    protected = b'{ "schemaVersion": 2, "payload": ["preserve container"] }'
    slot.write_bytes(protected)
    read = lambda: json.loads(slot.read_bytes())["payload"]
    with playing(player, project, resources, profile, output / "retry",
                 on_ready=lambda _, call: control.update(call=call)) as (click, _, _, screenshot):
        click("mode-choice"); click("room-voice-talk")
        control["call"]("wait-frames", frames=180)
        assert slot.read_bytes() == protected
        screenshot("checkpoint-blocked.png")
        save(profile, project, "work-conversation", [1,1,1,0])
        control["call"]("wait-frames", frames=90)
        assert read() == [1,1,2,0], read()
        stored = slot.read_bytes()
        external = b'{ "schemaVersion": 1, "payload": [3, ["preserve future content"]] }'
        slot.write_bytes(external)
        click("room-choice-option-1")
        control["call"]("wait-frames", frames=180)
        assert slot.read_bytes() == external
        slot.write_bytes(stored)
        control["call"]("wait-frames", frames=90)
        assert read() == [1,0,0,0], read()
        screenshot("checkpoint-recovered.png")
    with playing(player, project, resources, profile, output / "reopen", 1,
                 on_ready=lambda _, call: control.update(call=call)) as (click, _, _, screenshot):
        assert read() == [1,0,0,0]
        click("mode-choice"); click("room-voice-talk")
        control["call"]("wait-frames", frames=180)
        assert read() == [1,1,2,0]
        screenshot("reopened.png")
    for stage in ("retry", "reopen"):
        log = (output / stage / "stderr.log").read_text(encoding="utf-8")
        assert 'Insert instance layer "VK_LAYER_KHRONOS_validation"' in log
        assert "VUID-" not in log and "SYNC-HAZARD" not in log
    result = {"playerSHA256": digest(player), "controllerSHA256": digest(source / "work-choice.lua"),
              "protectedContainerVersion": 2, "protectedPayloadVersion": 3,
              "latestStageAfterRetry": [1,0,0,0], "reopenedStage": read(),
              "independentPlayers": 2, "liveAllocationsAfterExit": 0}
    write(output / "verification.json", result)
    print(json.dumps(result))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("player", "resources", "source", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    verify(args.player, args.resources, args.source, args.output)
