"""Replay original story music ownership through a failed track and recovery."""
import argparse
import json
import os
from pathlib import Path
from test_notes_player import playing
from test_original_music_player import fixture as music_fixture, digest, write

DRIVER = '''
local music, playlist, voice, started
function init()
    playlist=self:find("room-music"); voice=self:find("opening-story-music")
    music=new_story_music(self,{Original="assets:/tone-1.wav"})
end
function update(dt)
    if not started then assert(playlist:try_audio_play()); started=true end
    if self:pressed("story-play") then
        self:ui_text("story-status",music:play({"Original",".6","0",".25"}) and "Playing" or "Unavailable")
    end
    if self:pressed("story-finish") then music:finish() end
    music:update(dt)
    if self:pressed("story-probe") then
        assert(self:try_write_save("music-probe",{1,playlist:audio_playing(),playlist:audio_position(),voice:audio_playing()}))
    end
end
'''


def verify(player, resources, source, output):
    os.environ["VK_INSTANCE_LAYERS"] = "VK_LAYER_KHRONOS_validation"
    os.environ["VK_LOADER_DEBUG"] = "layer"
    output.mkdir()
    project = music_fixture(output / "fixture", source.with_name("music.lua"))
    assets = project.parent / "assets"
    (assets / "story.lua").write_text(source.read_text(encoding="utf-8") + DRIVER, encoding="utf-8")
    (assets / "story.rml").write_text('''<rml><head><style>
body {font-family:LatoLatin;margin:36dp;color:white;} #panel {padding:20dp;background-color:#263b45;width:520dp;}
button {padding:8dp;margin:4dp;background-color:#406677;}
</style></head><body><div id="panel"><div id="story-status"></div>
<button id="story-play" data-action="story-play">Story track</button>
<button id="story-finish" data-action="story-finish">Finish</button>
<button id="story-probe" data-action="story-probe">Inspect</button></div></body></rml>''', encoding="utf-8")
    descriptor = json.loads(project.read_bytes())
    descriptor["runtime"]["input"] = {name: [] for name in ("story-play", "story-finish", "story-probe")}
    write(project, descriptor)
    level = json.loads((assets / "room.azurelevel").read_bytes())
    components = level["nodes"][0]["components"]
    components.pop("azure.audio-source")
    components["azure.script"]["data"]["asset"] = "assets:/story.lua"
    components["azure.game-ui"]["data"]["asset"] = "assets:/story.rml"
    for name, asset, loop in (("room-music", "tone-2.wav", True), ("opening-story-music", "tone-1.wav", False)):
        level["nodes"].append({"id": name, "components": {"azure.audio-source": {
            "type": "azure.audio-source", "version": 1, "data": {
                "asset": "assets:/" + asset, "autoplay": False, "loop": loop, "volume": .2}}}})
    write(assets / "room.azurelevel", level)
    tone = assets / "tone-1.wav"
    original = tone.read_bytes(); tone.write_bytes(original[:16])
    profile, control = output / "profile", {}
    try:
        with playing(player, project, resources, profile, output / "recover", 2,
                     on_ready=lambda _, call: control.update(call=call),
                     expected_presentation_errors=("Truncated WAV container",)) as (click, _, _, screenshot):
            def probe():
                click("story-probe")
                path, = profile.glob("project-*/save-music-probe.json")
                return json.loads(path.read_bytes())["payload"]

            click("story-play")
            failed = probe()
            assert failed[1] and not failed[3] and failed[2] > 0, failed
            assert json.loads(control["call"]("query", name="game.state"))["audioStarts"] == 1
            screenshot("playlist-restored.png")
            tone.write_bytes(original)
            click("story-play")
            paused = probe()
            assert not paused[1] and paused[3], paused
            control["call"]("wait-frames", frames=10)
            held = probe()
            assert held[2] == paused[2] and not held[1], (held, paused)
            click("story-finish")
            control["call"]("wait-frames", frames=120)
            finished = probe()
            assert finished[1] and not finished[3], finished
            screenshot("story-finished.png")
    finally:
        tone.write_bytes(original)
    log = (output / "recover/stderr.log").read_text(encoding="utf-8")
    assert 'Insert instance layer "VK_LAYER_KHRONOS_validation"' in log
    assert "VUID-" not in log and "SYNC-HAZARD" not in log
    result = {"playerSHA256": digest(player), "controllerSHA256": digest(source),
              "failedTrack": failed, "pausedPlaylist": paused, "finishedTrack": finished,
              "audioStarts": 2, "independentPlayers": 1, "liveAllocationsAfterExit": 0}
    write(output / "verification.json", result)
    print(json.dumps(result))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("player", "resources", "source", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    verify(args.player, args.resources, args.source, args.output)
