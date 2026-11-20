"""Replay timer sound preview recovery using production binding and original audio."""
import argparse
import json
import os
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.npr_room.timer_sound_room_settings import bind_program, controls, LABEL_KEYS, FIELDS, STYLES
from test_original_timer_audio_player import fixture as timer_fixture
from test_original_music_player import digest, write
from test_notes_player import playing


def fixture(root, source):
    project = timer_fixture(root, source / "pomodoro.lua")
    assets = root / "assets"
    strings = {value: dict.fromkeys(FIELDS, name.title()) for name, value in LABEL_KEYS.items()}
    actions, markup = controls(strings, 1)
    descriptor = json.loads(project.read_bytes())
    descriptor["runtime"]["input"].update(actions)
    write(project, descriptor)
    labels = "local timer_sound_labels = {" + ",".join(name + "={" + json.dumps(name.title()) + "}"
                                                       for name in LABEL_KEYS) + "}\n"
    (assets / "timer.lua").write_text(bind_program((source / "pomodoro.lua").read_text(encoding="utf-8"), labels, 1), encoding="utf-8")
    ui = (assets / "timer.rml").read_text(encoding="utf-8")
    ui = ui.replace("</style>", STYLES + "#panel { width: 650dp; }</style>")
    ui = ui.replace("</div></body>", markup + "</div></body>")
    (assets / "timer.rml").write_text(ui, encoding="utf-8")
    level = json.loads((assets / "room.azurelevel").read_bytes())
    for name in ("timer-start-b", "timer-start-c", "timer-finish-b", "timer-finish-c"):
        node = json.loads(json.dumps(level["nodes"][1]))
        node["id"] = name
        level["nodes"].append(node)
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
    expected = [1, [2, 50, 0, 1, 50, 0], [1, 50, 0, 1, 50, 0]]
    try:
        with playing(player, project, resources, profile, output / "recover",
                     on_ready=lambda _, call: control.update(call=call),
                     expected_presentation_errors=("Truncated WAV container",)) as (click, _, _, screenshot):
            click("timer-sound-toggle")
            click("timer-sound-start-2")
            slot, = profile.glob("project-*/save-timer-sound-settings.json")
            assert json.loads(slot.read_bytes())["payload"] == expected
            state = json.loads(control["call"]("query", name="game.state"))
            assert not state["scriptErrors"] and state["audioStarts"] == 0, state
            screenshot("preview-unavailable.png")
            tone.write_bytes(original)
            click("timer-sound-start-2")
            state = json.loads(control["call"]("query", name="game.state"))
            assert state["audioStarts"] == 1 and not state["scriptErrors"], state
            assert json.loads(slot.read_bytes())["payload"] == expected
            screenshot("preview-recovered.png")
    finally:
        tone.write_bytes(original)
    with playing(player, project, resources, profile, output / "reopen", 2) as (click, _, _, screenshot):
        click("timer-sound-toggle")
        click("timer-sound-start-2")
        assert json.loads(slot.read_bytes())["payload"] == expected
        screenshot("reopened.png")
    for folder in ("recover", "reopen"):
        text = (output / folder / "stderr.log").read_text(encoding="utf-8")
        assert 'Insert instance layer "VK_LAYER_KHRONOS_validation"' in text
        assert "VUID-" not in text and "SYNC-HAZARD" not in text
    result = {"playerSHA256": digest(player), "productionScriptSHA256": {
        name: digest(source / name) for name in ("pomodoro.lua", "timer-sound-settings.lua", "timer-sound-menu.lua")},
        "persistedPreferences": expected, "recoveredAudioStarts": state["audioStarts"],
        "independentPlayers": 2, "scriptErrors": [], "liveAllocationsAfterExit": 0}
    write(output / "verification.json", result)
    print(json.dumps(result))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("player", "resources", "source", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    verify(args.player, args.resources, args.source, args.output)
