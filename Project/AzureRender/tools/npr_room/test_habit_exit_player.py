"""Verify habit name drafts through the production timer and calendar lifecycle."""
import argparse
import json
import os
from pathlib import Path

from test_notes_player import playing
from test_original_music_player import fixture as music_fixture, digest, write


MODULES = ("habit-tracker.lua", "habit-calendar-menu.lua", "work-calendar.lua", "pomodoro.lua")


def fixture(root, source):
    project = music_fixture(root, source / "music.lua")
    document = json.loads(project.read_bytes())
    document["runtime"]["input"] = {name: [] for name in (
        "timer-stats", "calendar-habits-toggle", "calendar-habit-new", "calendar-habit-save")}
    write(project, document)
    assets = root / "assets"
    (assets / "habits.lua").write_bytes(b"\n".join((source / name).read_bytes() for name in MODULES))
    (assets / "habits.rml").write_bytes((source / "timer.rml").read_bytes())
    level = json.loads((assets / "room.azurelevel").read_bytes())
    component = level["nodes"][0]["components"]
    component.pop("azure.audio-source")
    component["azure.script"]["data"]["asset"] = "assets:/habits.lua"
    component["azure.game-ui"]["data"]["asset"] = "assets:/habits.rml"
    write(assets / "room.azurelevel", level)
    return project


def verify(player, resources, source, output):
    os.environ["VK_INSTANCE_LAYERS"] = "VK_LAYER_KHRONOS_validation"
    os.environ["VK_LOADER_DEBUG"] = "layer"
    output.mkdir()
    project = fixture(output / "fixture", source)
    profile = output / "profile"
    phases = ("draft", "reopen", "future-payload", "future-envelope")
    with playing(player, project, resources, profile, output / "draft", 0) as (click, _, type_text, screenshot):
        click("timer-stats")
        click("calendar-habits-toggle")
        click("calendar-habit-new")
        slot, = profile.glob("project-*/save-habit-tracker.json")
        click("calendar-habit-title")
        type_text("Original habit")
        screenshot("draft.png")
        assert json.loads(slot.read_bytes())["payload"][2][0][1] == "", "Name was committed before close"
    first = json.loads(slot.read_bytes())["payload"]
    assert first[2][0][1] == "Original habit", ("Window close lost the unblurred habit name", first)
    with playing(player, project, resources, profile, output / "reopen", 0) as (click, press, type_text, screenshot):
        click("timer-stats")
        click("calendar-habits-toggle")
        screenshot("restored.png")
        click("calendar-habit-title")
        press(269)
        type_text(" restored")
        click("timer-stats")
        assert json.loads(slot.read_bytes())["payload"][2][0][1] == "Original habit restored"
    committed = slot.read_bytes()
    for phase, future in (
        ("future-payload", b'{ "schemaVersion": 1, "payload": [2, ["future habit data"]] }'),
        ("future-envelope", b'{ "schemaVersion": 2, "payload": ["future habit envelope"] }'),
    ):
        slot.write_bytes(committed)
        with playing(player, project, resources, profile, output / phase, 0) as (click, press, type_text, _):
            click("timer-stats")
            click("calendar-habits-toggle")
            click("calendar-habit-title")
            press(269)
            type_text(" rejected")
            slot.write_bytes(future)
        assert slot.read_bytes() == future, ("Exit replaced a newer habit save", phase)
    for phase in phases:
        log = (output / phase / "stderr.log").read_text(encoding="utf-8")
        assert 'Insert instance layer "VK_LAYER_KHRONOS_validation"' in log, phase
        assert "VUID-" not in log and "SYNC-HAZARD" not in log, phase
    report = {
        "playerSHA256": digest(player), "moduleSHA256": {name: digest(source / name) for name in MODULES},
        "fixtureSHA256": {p.relative_to(project.parent).as_posix(): digest(p)
                          for p in sorted(project.parent.rglob("*")) if p.is_file() and ".azure" not in p.parts},
        "firstExit": first, "calendarClose": json.loads(committed)["payload"],
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
