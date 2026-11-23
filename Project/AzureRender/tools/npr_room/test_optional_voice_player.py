"""Replay original room, work-choice and timer voice timelines through audio failure."""
import argparse
import json
import os
from pathlib import Path
from test_original_music_player import fixture as music_fixture, digest, write
from test_notes_player import playing


CONFIG = '''
local responses={{"assets:/tone-1.wav","Original greeting",2,0,.2}}
local work_choice_parameters={fullProbabilitySeconds=1,probability=1,availableSeconds=100}
local work_conversations={{{"assets:/tone-1.wav","Choose an original activity",2,0,.2},
    {{"Read",{"assets:/tone-1.wav","Read a new page",2,0,.2}}}}}
'''
DRIVER = '''
local banter, choice, timer, counts, mode
local wasBanter, wasChoice, wasTimer = false, false, false
function init()
    banter = new_room_voice()
    choice = new_work_choice(function(limit) return limit and 1 or 0 end)
    choice:recordWork(1); choice:complete()
    local line={asset="assets:/tone-1.wav",localization="original",text="Original timer line",seconds=2,
        bodyMotion=-1,speakBeforeMotion=true,animationCue=0}
    timer = new_timer_voice_action({groups={[4]={line}},motions={},rules={longWorkStart={seconds=1000,percent=0}},
        random=function(limit) return limit and 1 or 0 end}, function() end, function(value) return value end)
    local saved = self:read_save("voice-proof")
    counts = saved and saved[1]==1 and saved or {1,0,0,0}
    mode = "banter"
end
function update(dt)
    local changed = false
    if self:pressed("mode-banter") then mode="banter" end
    if self:pressed("mode-choice") then mode="choice" end
    if self:pressed("timer-trigger") then timer:trigger("work",false,1,60,true,false,"idle") end
    if mode=="banter" then
        local active=banter(dt,true,false,false)
        if wasBanter and not active then counts[2]=counts[2]+1; changed=true end
        wasBanter=active
    else
        local active=choice:step(dt,true,false)
        if wasChoice and not active then counts[3]=counts[3]+1; changed=true end
        wasChoice=active
    end
    timer:advanceClock(dt,0)
    local active=timer:update(dt,false)
    if wasTimer and not active then counts[4]=counts[4]+1; changed=true end
    wasTimer=active
    if changed then assert(self:try_write_save("voice-proof",counts)) end
    self:ui_text("voice-counts",string.format("Greeting %d / Choice %d / Timer %d",counts[2],counts[3],counts[4]))
end
'''


def fixture(root, source):
    project = music_fixture(root, source / "music.lua")
    assets = root / "assets"
    names = ("room-voice.lua", "work-choice.lua", "timer-voice-selector.lua", "timer-voice-action.lua")
    program = CONFIG + "\n" + "\n".join((source / name).read_text(encoding="utf-8") for name in names) + DRIVER
    (assets / "voices.lua").write_text(program, encoding="utf-8")
    descriptor = json.loads(project.read_bytes())
    descriptor["runtime"]["input"] = {name: [] for name in
        ("mode-banter", "mode-choice", "room-voice-talk", "room-choice-option-1", "timer-trigger")}
    write(project, descriptor)
    (assets / "voices.rml").write_text('''<rml><head><style>
body { font-family: LatoLatin; margin: 36dp; color: #fff2ce; }
#panel { width: 600dp; padding: 20dp; background-color: #263b45; }
button { margin: 4dp; padding: 8dp; background-color: #406677; }
</style></head><body><div id="panel"><div id="voice-counts"></div>
<button id="mode-banter" data-action="mode-banter">Greeting</button>
<button id="mode-choice" data-action="mode-choice">Choice</button>
<button id="room-voice-talk" data-action="room-voice-talk">Talk</button>
<button id="timer-trigger" data-action="timer-trigger">Start timer voice</button>
<div id="room-voice-caption"><div id="room-voice-line"></div></div>
<div id="room-choice-caption"><div id="room-choice-line"></div></div>
<div id="room-choice-options"><div id="room-choice-question"></div>
<button id="room-choice-option-1" data-action="room-choice-option-1">Read</button></div>
<div id="room-choice-status"></div>
<div id="timer-voice-caption"><div id="timer-voice-line"></div></div>
</div></body></rml>''', encoding="utf-8")
    level = json.loads((assets / "room.azurelevel").read_bytes())
    components = level["nodes"][0]["components"]
    audio = components.pop("azure.audio-source")
    components["azure.script"]["data"]["asset"] = "assets:/voices.lua"
    components["azure.game-ui"]["data"]["asset"] = "assets:/voices.rml"
    for name in ("room-voice", "room-choice-voice", "room-timer-voice"):
        level["nodes"].append({"id": name, "components": {"azure.audio-source": audio}})
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

    def read(slot):
        path, = profile.glob(f"project-*/save-{slot}.json")
        return json.loads(path.read_bytes())["payload"]

    def wait():
        control["call"]("wait-frames", frames=180)

    try:
        with playing(player, project, resources, profile, output / "missing",
                     on_ready=lambda _, call: control.update(call=call),
                     expected_presentation_errors=("Truncated WAV container",)) as (click, _, _, screenshot):
            click("room-voice-talk"); wait()
            assert read("voice-proof") == [1,1,0,0]
            click("timer-trigger"); wait()
            assert read("voice-proof") == [1,1,0,1]
            click("mode-choice"); click("room-voice-talk"); wait()
            assert read("work-conversation") == [1,1,2,0]
            state = json.loads(control["call"]("query", name="game.state"))
            assert state["audioStarts"] == 0 and not state["scriptErrors"], state
            screenshot("missing-voice-options.png")
        with playing(player, project, resources, profile, output / "recover",
                     on_ready=lambda _, call: control.update(call=call),
                     expected_presentation_errors=("Truncated WAV container",)) as (click, _, _, screenshot):
            click("mode-choice"); click("room-choice-option-1"); wait()
            assert read("voice-proof") == [1,1,1,1]
            assert read("work-conversation") == [1,0,0,0]
            tone.write_bytes(original)
            click("mode-banter"); click("room-voice-talk"); wait()
            click("timer-trigger"); wait()
            click("mode-choice"); click("room-voice-talk"); wait()
            click("room-choice-option-1"); wait()
            assert read("voice-proof") == [1,2,2,2]
            state = json.loads(control["call"]("query", name="game.state"))
            assert state["audioStarts"] == 4 and not state["scriptErrors"], state
            screenshot("recovered-voices.png")
    finally:
        tone.write_bytes(original)
    for stage in ("missing", "recover"):
        log = (output / stage / "stderr.log").read_text(encoding="utf-8")
        assert 'Insert instance layer "VK_LAYER_KHRONOS_validation"' in log
        assert "VUID-" not in log and "SYNC-HAZARD" not in log
    result = {"playerSHA256": digest(player), "sourceSHA256": {name: digest(source / name) for name in
        ("room-voice.lua", "work-choice.lua", "timer-voice-action.lua")}, "completedTimelines": read("voice-proof"),
        "conversationProgress": read("work-conversation"), "recoveredAudioStarts": 4,
        "independentPlayers": 2, "liveAllocationsAfterExit": 0}
    write(output / "verification.json", result)
    print(json.dumps(result))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("player", "resources", "source", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    verify(args.player, args.resources, args.source, args.output)
