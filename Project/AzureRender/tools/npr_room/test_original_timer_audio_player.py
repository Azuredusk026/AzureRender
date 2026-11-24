"""Replay timer persistence while nonessential original audio is unavailable."""
import argparse
import json
import os
from pathlib import Path
from test_notes_player import playing
from test_original_music_player import fixture as music_fixture, digest, write


def fixture(root, source):
    project = music_fixture(root, source.with_name("music.lua"))
    descriptor = json.loads(project.read_bytes())
    descriptor["runtime"]["input"] = {name: [] for name in ("timer-mode", "timer-toggle", "timer-reset")}
    write(project, descriptor)
    assets = root / "assets"
    (assets / "timer.lua").write_bytes(source.read_bytes())
    (assets / "timer.rml").write_text('''<rml><head><style>
body { font-family: LatoLatin; margin: 36dp; color: #fff2ce; }
#panel { padding: 20dp; width: 420dp; background-color: #263b45; }
#timer-value { font-size: 34dp; } button { margin: 4dp; padding: 8dp; background-color: #406677; }
</style></head><body><div id="panel"><div id="timer-settings"></div>
<div id="timer-phase"></div><div id="timer-value"></div>
<button id="timer-mode" data-action="timer-mode">Mode</button>
<button id="timer-toggle" data-action="timer-toggle">Start / pause</button>
<button id="timer-reset" data-action="timer-reset">Finish</button>
</div></body></rml>''', encoding="utf-8")
    level = json.loads((assets / "room.azurelevel").read_bytes())
    controller = level["nodes"][0]["components"]
    controller.pop("azure.audio-source")
    controller["azure.script"]["data"]["asset"] = "assets:/timer.lua"
    controller["azure.game-ui"]["data"]["asset"] = "assets:/timer.rml"
    for name in ("timer-start", "timer-pause", "timer-finish", "settings-click", "settings-cancel"):
        level["nodes"].append({"id": name, "components": {"azure.audio-source": {
            "type": "azure.audio-source", "version": 1, "data": {
                "asset": "assets:/tone-1.wav", "volume": .5, "autoplay": False, "loop": False}}}})
    write(assets / "room.azurelevel", level)
    return project


def verify(player, resources, source, output):
    os.environ["VK_INSTANCE_LAYERS"] = "VK_LAYER_KHRONOS_validation"
    os.environ["VK_LOADER_DEBUG"] = "layer"
    output.mkdir()
    project = fixture(output / "fixture", source)
    tone = project.parent / "assets/tone-1.wav"
    original = tone.read_bytes()
    tone.write_bytes(original[:16])
    profile, control = output / "profile", {}
    try:
        with playing(player, project, resources, profile, output / "recover",
                     on_ready=lambda _, call: control.update(call=call),
                     expected_presentation_errors=("Truncated WAV container",)) as (click, _, _, screenshot):
            click("timer-mode")
            click("timer-toggle")
            control["call"]("wait-frames", frames=150)
            click("timer-toggle")
            slot, = profile.glob("project-*/save-countup-session.json")
            paused = json.loads(slot.read_bytes())["payload"]
            assert paused[0] == 1 and paused[1] >= 1 and paused[2] == 0, paused
            state = json.loads(control["call"]("query", name="game.state"))
            assert state["audioStarts"] == 0 and not state["scriptErrors"], state
            screenshot("paused-without-cues.png")
            tone.write_bytes(original)
            click("timer-toggle")
            control["call"]("wait-frames", frames=90)
            click("timer-reset")
            finished = json.loads(slot.read_bytes())["payload"]
            assert finished[0] == 1 and finished[1] == 0 and finished[2] >= paused[1] + 1, finished
            state = json.loads(control["call"]("query", name="game.state"))
            assert state["audioStarts"] >= 2 and not state["scriptErrors"], state
            screenshot("finished-with-cues.png")
    finally:
        tone.write_bytes(original)
    with playing(player, project, resources, profile, output / "reopen") as (_, _, _, screenshot):
        assert json.loads(slot.read_bytes())["payload"] == finished
        screenshot("reopened.png")
    for folder in ("recover", "reopen"):
        text = (output / folder / "stderr.log").read_text(encoding="utf-8")
        assert 'Insert instance layer "VK_LAYER_KHRONOS_validation"' in text
        assert "VUID-" not in text and "SYNC-HAZARD" not in text
    result = {"playerSHA256": digest(player), "controllerSHA256": digest(source),
              "originalToneSHA256": digest(tone), "pausedWithoutCues": paused, "finished": finished,
              "recoveredAudioStarts": state["audioStarts"], "independentPlayers": 2,
              "scriptErrors": [], "liveAllocationsAfterExit": 0}
    write(output / "verification.json", result)
    print(json.dumps(result))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("player", "resources", "source", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    verify(args.player, args.resources, args.source, args.output)
