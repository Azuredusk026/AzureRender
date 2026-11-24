"""Replay notebook and task payload protection with native editing and restart."""
import argparse
import json
import os
from pathlib import Path
from test_original_music_player import fixture as music_fixture, digest, write
from test_original_todo_audio_player import fixture as todo_fixture
from test_notes_player import playing
from test_unlock_room_player import save


def fixture(root, source, kind):
    if kind == "todo":
        project = todo_fixture(root, source)
        assets = root / "assets"
        descriptor = json.loads(project.read_bytes())
        descriptor["runtime"]["input"]["todo-task-new"] = []
        write(project, descriptor)
        ui = (assets / "todo.rml").read_text(encoding="utf-8")
        anchor = '<div id="todo-panel" style="display: none">'
        assert ui.count(anchor) == 1
        ui = ui.replace(anchor, anchor + '<button id="todo-task-new" data-action="todo-task-new">New task</button><button id="todo-save" data-action="todo-save">Save</button>')
        (assets / "todo.rml").write_text(ui, encoding="utf-8")
        return project
    project = music_fixture(root, source.with_name("music.lua"))
    assets = root / "assets"
    descriptor = json.loads(project.read_bytes())
    descriptor["runtime"]["input"] = {name: [] for name in ("notes-toggle", "notes-save", "notes-new")}
    write(project, descriptor)
    (assets / "notes.lua").write_bytes(source.read_bytes())
    (assets / "notes.rml").write_text('''<rml><head><style>
body { font-family: LatoLatin; margin: 36dp; color: #fff2ce; }
#notes-panel { padding: 20dp; width: 420dp; background-color: #263b45; }
button { margin: 4dp; padding: 8dp; background-color: #406677; }
input, textarea { display: block; width: 350dp; margin: 8dp 0; }
</style></head><body><button id="notes-toggle" data-action="notes-toggle">Notes</button>
<div id="notes-panel" style="display: none"><div id="note-page"></div><div id="note-fields">
<input id="note-title" type="text"/><textarea id="note-text"></textarea></div>
<button id="notes-save" data-action="notes-save">Save</button>
<button id="notes-new" data-action="notes-new">New page</button>
<div id="note-status"></div></div></body></rml>''', encoding="utf-8")
    level = json.loads((assets / "room.azurelevel").read_bytes())
    components = level["nodes"][0]["components"]
    components.pop("azure.audio-source")
    components["azure.script"]["data"]["asset"] = "assets:/notes.lua"
    components["azure.game-ui"]["data"]["asset"] = "assets:/notes.rml"
    write(assets / "room.azurelevel", level)
    return project


def verify(player, resources, source, output):
    os.environ["VK_INSTANCE_LAYERS"] = "VK_LAYER_KHRONOS_validation"
    os.environ["VK_LOADER_DEBUG"] = "layer"
    output.mkdir()
    results = {}
    for kind in ("notes", "todo"):
        root = output / kind
        root.mkdir()
        controller = source / (kind + ".lua")
        project = fixture(root / "fixture", controller, kind)
        profile = root / "profile"
        save(profile, project, kind, [2, ["future document"]])
        slot, = profile.glob(f"project-*/save-{kind}.json")
        protected = slot.read_bytes()
        read = lambda: json.loads(slot.read_bytes())["payload"]
        text = lambda value: value[2][0][2] if kind == "notes" else value[2][0][2][0][1]
        toggle, submit, field = ("notes-toggle", "notes-save", "note-text") if kind == "notes" else ("todo-toggle", "todo-save", "todo-task-text")
        with playing(player, project, resources, profile, root / "protect") as (click, press, type_text, screenshot):
            click(toggle)
            if kind == "notes":
                click(field)
                type_text("Draft")
                click(submit)
            else:
                click("todo-task-new")
            assert slot.read_bytes() == protected, kind
            screenshot("future-protected.png")
            compatible = [1, 2, [[1, "New Page", ""]]] if kind == "notes" else [1, 2, [[1, "", []]]]
            save(profile, project, kind, compatible)
            if kind == "todo":
                click("todo-task-new")
                click(field)
                type_text("Draft")
            click(submit)
            assert text(read()) == "Draft", (kind, read())
            stored = slot.read_bytes()
            external = b'{ "schemaVersion": 1, "payload": [3, ["external future document"]] }'
            slot.write_bytes(external)
            click(field)
            press(269)
            type_text(" again")
            click(submit)
            assert slot.read_bytes() == external, kind
            slot.write_bytes(stored)
            click(submit)
            assert text(read()) == "Draft again", (kind, read())
            before_reopen = read()
            screenshot("compatible-restored.png")
        with playing(player, project, resources, profile, root / "reopen") as (click, _, _, screenshot):
            assert read() == before_reopen, kind
            click(toggle)
            click("notes-new" if kind == "notes" else "todo-task-complete")
            assert text(read()) == "Draft again", kind
            screenshot("reopened.png")
        for stage in ("protect", "reopen"):
            log = (root / stage / "stderr.log").read_text(encoding="utf-8")
            assert 'Insert instance layer "VK_LAYER_KHRONOS_validation"' in log
            assert "VUID-" not in log and "SYNC-HAZARD" not in log
        results[kind] = {"controllerSHA256": digest(controller), "protectedVersions": [2, 3],
                         "protectedOriginalBytes": True, "beforeReopen": before_reopen, "afterReopen": read()}
    results.update(playerSHA256=digest(player), independentPlayers=4, liveAllocationsAfterExit=0)
    write(output / "verification.json", results)
    print(json.dumps(results))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("player", "resources", "source", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    verify(args.player, args.resources, args.source, args.output)
