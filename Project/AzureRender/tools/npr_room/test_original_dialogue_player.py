"""Replay original text dialogue through unavailable voice, recovery and restart."""
import argparse
import json
import os
from pathlib import Path
from test_notes_player import playing
from test_original_music_player import fixture as music_fixture, digest, write


DRIVER = '''
local dialogue, completed
function init()
    dialogue = new_story_dialogue(self, {En=.025}, function() return "En" end)
    local saved = self:read_save("dialogue-progress")
    completed = saved and saved[1] == 1 and saved[2] or 0
end
function update(dt)
    if self:pressed("dialogue-show") then
        dialogue:show({translations={En="A quiet original room."}, asset="tone-1.wav", frames=96000, sampleRate=48000})
    end
    dialogue:update(dt)
    self:ui_text("dialogue-state", dialogue:ready() and "Ready" or "Waiting")
    if self:pressed("dialogue-next") and dialogue:ready() then
        completed = completed + 1
        assert(self:try_write_save("dialogue-progress", {1, completed}))
        dialogue:clear()
    end
    self:ui_text("dialogue-count", tostring(completed))
end
'''


def fixture(root, source):
    project = music_fixture(root, source.with_name("music.lua"))
    assets = root / "assets"
    descriptor = json.loads(project.read_bytes())
    descriptor["runtime"]["input"] = {name: [] for name in ("dialogue-show", "dialogue-next")}
    write(project, descriptor)
    (assets / "dialogue.lua").write_text(source.read_text(encoding="utf-8") + DRIVER, encoding="utf-8")
    (assets / "dialogue.rml").write_text('''<rml><head><style>
body { font-family: LatoLatin; margin: 36dp; color: #fff2ce; }
#panel { padding: 20dp; width: 520dp; background-color: #263b45; }
button { margin: 4dp; padding: 8dp; background-color: #406677; }
</style></head><body><div id="panel"><div id="opening-story-caption"><div id="opening-story-line"></div></div>
<div id="dialogue-state"></div><div id="dialogue-count"></div>
<button id="dialogue-show" data-action="dialogue-show">Speak</button>
<button id="dialogue-next" data-action="dialogue-next">Continue</button></div></body></rml>''', encoding="utf-8")
    level = json.loads((assets / "room.azurelevel").read_bytes())
    components = level["nodes"][0]["components"]
    audio = components.pop("azure.audio-source")
    components["azure.script"]["data"]["asset"] = "assets:/dialogue.lua"
    components["azure.game-ui"]["data"]["asset"] = "assets:/dialogue.rml"
    level["nodes"].append({"id": "opening-story-voice", "components": {"azure.audio-source": audio}})
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
            click("dialogue-show")
            click("dialogue-next")
            assert not list(profile.glob("project-*/save-dialogue-progress.json")), "Early advance must remain rejected"
            control["call"]("wait-frames", frames=150)
            screenshot("text-ready-without-voice.png")
            click("dialogue-next")
            slot, = profile.glob("project-*/save-dialogue-progress.json")
            assert json.loads(slot.read_bytes())["payload"] == [1, 1]
            tone.write_bytes(original)
            click("dialogue-show")
            control["call"]("wait-frames", frames=180)
            click("dialogue-next")
            assert json.loads(slot.read_bytes())["payload"] == [1, 2]
            state = json.loads(control["call"]("query", name="game.state"))
            assert state["audioStarts"] == 1 and not state["scriptErrors"], state
            screenshot("voice-recovered.png")
    finally:
        tone.write_bytes(original)
    with playing(player, project, resources, profile, output / "reopen", 1,
                 on_ready=lambda _, call: control.update(call=call)) as (click, _, _, screenshot):
        assert json.loads(slot.read_bytes())["payload"] == [1, 2]
        click("dialogue-show")
        control["call"]("wait-frames", frames=180)
        click("dialogue-next")
        assert json.loads(slot.read_bytes())["payload"] == [1, 3]
        screenshot("reopened.png")
    for stage in ("recover", "reopen"):
        text = (output / stage / "stderr.log").read_text(encoding="utf-8")
        assert 'Insert instance layer "VK_LAYER_KHRONOS_validation"' in text
        assert "VUID-" not in text and "SYNC-HAZARD" not in text
    result = {"playerSHA256": digest(player), "controllerSHA256": digest(source),
              "earlyAdvanceRejected": True, "missingVoiceAdvance": [1, 1], "reopenedProgress": [1, 3],
              "recoveredAudioStarts": 1, "independentPlayers": 2, "liveAllocationsAfterExit": 0}
    write(output / "verification.json", result)
    print(json.dumps(result))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("player", "resources", "source", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    verify(args.player, args.resources, args.source, args.output)
