"""Replay task drafts across native window close, restart and rejected exit saves."""
import argparse
import json
import os
from pathlib import Path

from test_notes_player import playing
from test_original_todo_audio_player import fixture
from test_original_music_player import digest, write
from test_unlock_room_player import save


def verify(player, resources, source, output):
    os.environ["VK_INSTANCE_LAYERS"] = "VK_LAYER_KHRONOS_validation"
    os.environ["VK_LOADER_DEBUG"] = "layer"
    output.mkdir()
    project = fixture(output / "fixture", source)
    profile = output / "profile"
    initial = [1, 3, [[1, "", [[2, "", "", ""]]]]]
    save(profile, project, "todo", initial)
    slot, = profile.glob("project-*/save-todo.json")
    read = lambda: json.loads(slot.read_bytes())["payload"]
    phases = ["draft", "reopen", "invalid-date", "future-payload", "future-envelope"]
    with playing(player, project, resources, profile, output / "draft", 0) as (click, _, type_text, screenshot):
        click("todo-toggle")
        click("todo-list-title")
        type_text("Original list")
        click("todo-task-text")
        type_text("First line\nSecond line")
        click("todo-task-due")
        type_text("2028-02-29")
        screenshot("draft.png")
        assert read() == initial, "Draft should remain unsubmitted before window close"
    first = read()
    assert first == [1, 3, [[1, "Original list", [[2, "First line\nSecond line", "", "2028-02-29"]]]]], (
        "Window close lost the task draft", first)
    with playing(player, project, resources, profile, output / "reopen", 0) as (click, press, type_text, screenshot):
        click("todo-toggle")
        screenshot("restored.png")
        click("todo-task-text")
        press(264)
        press(269)
        type_text(" restored")
        assert read() == first
    restored = read()
    assert restored[2][0][2][0] == [2, "First line\nSecond line restored", "", "2028-02-29"], restored
    committed = slot.read_bytes()
    with playing(player, project, resources, profile, output / "invalid-date", 0) as (click, press, type_text, screenshot):
        click("todo-toggle")
        click("todo-task-text")
        press(269)
        type_text(" rejected")
        click("todo-task-due")
        press(269)
        for _ in "2028-02-29":
            press(259)
        type_text("2027-02-29")
        screenshot("invalid-date.png")
    assert slot.read_bytes() == committed, "Exit accepted an invalid task date"
    for phase, future in (
        ("future-payload", b'{ "schemaVersion": 1, "payload": [2, ["future task data"]] }'),
        ("future-envelope", b'{ "schemaVersion": 2, "payload": ["future envelope"] }'),
    ):
        slot.write_bytes(committed)
        with playing(player, project, resources, profile, output / phase, 0) as (click, press, type_text, _):
            click("todo-toggle")
            click("todo-task-text")
            press(269)
            type_text(" rejected")
            slot.write_bytes(future)
        assert slot.read_bytes() == future, ("Exit replaced a newer save", phase)
    for phase in phases:
        log = (output / phase / "stderr.log").read_text(encoding="utf-8")
        assert 'Insert instance layer "VK_LAYER_KHRONOS_validation"' in log, phase
        assert "VUID-" not in log and "SYNC-HAZARD" not in log, phase
    report = {
        "playerSHA256": digest(player), "controllerSHA256": digest(source),
        "fixtureSHA256": {p.relative_to(project.parent).as_posix(): digest(p)
                          for p in sorted(project.parent.rglob("*")) if p.is_file() and ".azure" not in p.parts},
        "firstExit": first, "secondExit": restored, "invalidDatePreserved": True,
        "futurePayloadPreserved": True, "futureEnvelopePreserved": True,
        "independentPlayers": len(phases), "liveAllocationsAfterExit": 0,
    }
    write(output / "verification.json", report)
    print(json.dumps(report))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("player", "resources", "source", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    verify(args.player, args.resources, args.source, args.output)
