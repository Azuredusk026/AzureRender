"""Replay task completion and restoration while original feedback audio fails."""
import argparse
import json
import os
from pathlib import Path
from test_notes_player import playing
from test_original_music_player import fixture as music_fixture, digest, write
from test_unlock_room_player import save


def fixture(root, source):
    project = music_fixture(root, source.with_name("music.lua"))
    descriptor = json.loads(project.read_bytes())
    descriptor["runtime"]["input"] = {name: [] for name in ("todo-toggle", "todo-task-complete", "todo-save")}
    write(project, descriptor)
    assets = root / "assets"
    (assets / "todo.lua").write_bytes(source.read_bytes())
    (assets / "todo.rml").write_text('''<rml><head><style>
body { font-family: LatoLatin; margin: 36dp; color: #fff2ce; }
#todo-panel { padding: 20dp; width: 420dp; background-color: #263b45; }
button { margin: 4dp; padding: 8dp; background-color: #406677; }
input, textarea { display: block; width: 350dp; margin: 8dp 0; }
</style></head><body><button id="todo-toggle" data-action="todo-toggle">Tasks</button>
<div id="todo-panel" style="display: none"><div id="todo-list-position"></div>
<input id="todo-list-title" type="text"/><div id="todo-progress"></div>
<div id="todo-task-details"><textarea id="todo-task-text"></textarea>
<input id="todo-task-due" type="text"/><div id="todo-task-position"></div><div id="todo-task-state"></div>
<button id="todo-task-complete" data-action="todo-task-complete">Complete</button></div>
<div id="todo-status"></div></div></body></rml>''', encoding="utf-8")
    level = json.loads((assets / "room.azurelevel").read_bytes())
    component = level["nodes"][0]["components"]
    component.pop("azure.audio-source")
    component["azure.script"]["data"]["asset"] = "assets:/todo.lua"
    component["azure.game-ui"]["data"]["asset"] = "assets:/todo.rml"
    for name in ("settings-click", "settings-cancel", "todo-complete"):
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
    save(profile, project, "todo", [1, 3, [[1, "Original tasks", [[2, "Verify the room", "", ""]]]]])
    slot, = profile.glob("project-*/save-todo.json")
    read = lambda: json.loads(slot.read_bytes())["payload"]
    try:
        with playing(player, project, resources, profile, output / "recover",
                     on_ready=lambda _, call: control.update(call=call),
                     expected_presentation_errors=("Truncated WAV container",)) as (click, _, _, screenshot):
            click("todo-toggle")
            click("todo-task-complete")
            stamp = read()[2][0][2][0][2]
            assert len(stamp) == 14 and stamp.isdigit(), stamp
            screenshot("completed-without-cue.png")
            click("todo-task-complete")
            assert read()[2][0][2][0][2] == ""
            state = json.loads(control["call"]("query", name="game.state"))
            assert state["audioStarts"] == 0 and not state["scriptErrors"], state
            tone.write_bytes(original)
            click("todo-task-complete")
            completed = read()
            state = json.loads(control["call"]("query", name="game.state"))
            assert state["audioStarts"] == 1 and not state["scriptErrors"], state
            screenshot("completed-with-cue.png")
    finally:
        tone.write_bytes(original)
    with playing(player, project, resources, profile, output / "reopen", 1) as (click, _, _, screenshot):
        assert read() == completed
        click("todo-toggle")
        click("todo-task-complete")
        assert read()[2][0][2][0][2] == ""
        screenshot("restored.png")
    for folder in ("recover", "reopen"):
        text = (output / folder / "stderr.log").read_text(encoding="utf-8")
        assert 'Insert instance layer "VK_LAYER_KHRONOS_validation"' in text
        assert "VUID-" not in text and "SYNC-HAZARD" not in text
    result = {"playerSHA256": digest(player), "controllerSHA256": digest(source),
              "completedWithoutCue": stamp, "completedBeforeReopen": completed,
              "restored": read(), "independentPlayers": 2, "liveAllocationsAfterExit": 0}
    write(output / "verification.json", result)
    print(json.dumps(result))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("player", "resources", "source", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    verify(args.player, args.resources, args.source, args.output)
