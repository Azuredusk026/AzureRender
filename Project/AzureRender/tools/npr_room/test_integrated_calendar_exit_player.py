"""Replay work-time exit saves in an original host built by integrate_calendar."""
import argparse
import json
import os
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from test_habit_exit_player import fixture as habit_fixture
from test_notes_player import playing
from test_original_music_player import digest, write
from tools.npr_room.build_work_calendar import integrate_calendar


LEGACY_HOST = '''local workSlot = "work-calendar"
function new_work_calendar()
    local calendar = {}
    return calendar
end
local workCalendar, statsOpen
local function durationText(value) return tostring(value) end
function init()
    workCalendar = new_work_calendar()
    statsOpen = false
    self:ui_visible("work-stats", false)
end
function update()
        if self:pressed("timer-stats") then
            statsOpen = not statsOpen
            self:ui_visible("work-stats", statsOpen)
        end
        if statsOpen then
            local growth = workCalendar:progress()
        end
end
function shutdown()
    if workCalendar then workCalendar:flush() end
end
'''
LEGACY_UI = '''<rml><head><style>
body { width: 100%; height: 100%; font-family: LatoLatin; color: white; }
#work-stats { display: block; margin-top: 8dp; font-size: 13dp; }
</style></head><body><div id="timer" style="position: absolute; right: 16dp; top: 16dp; width: 300dp;">
<button id="timer-stats" data-action="timer-stats">Calendar</button>
<div id="work-stats"><span id="work-progression" style="display: none;"></span></div></div></body></rml>'''


def fixture(root, source):
    project = habit_fixture(root, source)
    descriptor = json.loads(project.read_bytes())
    script, markup = integrate_calendar(descriptor, LEGACY_UI, LEGACY_HOST, source)
    write(project, descriptor)
    (root / "assets/habits.lua").write_text(script, encoding="utf-8")
    (root / "assets/habits.rml").write_text(markup, encoding="utf-8")
    return project


def verify(player, resources, source, output):
    os.environ["VK_INSTANCE_LAYERS"] = "VK_LAYER_KHRONOS_validation"
    os.environ["VK_LOADER_DEBUG"] = "layer"
    output.mkdir()
    project = fixture(output / "fixture", source)
    profile = output / "profile"
    read = lambda slot: json.loads(slot.read_bytes())["payload"]
    seconds = lambda payload: sum(value for _, days in payload[1] for _, value in days)
    phases = ("draft", "reopen", "invalid-input", "future-payload", "future-envelope")
    with playing(player, project, resources, profile, output / "draft", 0) as (click, press, type_text, screenshot):
        click("timer-stats")
        click("calendar-edit-time")
        click("calendar-edit-hours")
        press(269)
        press(259)
        type_text("1")
        screenshot("draft.png")
        assert not list(profile.glob("project-*/save-work-calendar.json")), "Draft was saved before close"
    slots = list(profile.glob("project-*/save-work-calendar.json"))
    assert len(slots) == 1, "Closing the assembled host lost open work-time edits"
    slot, = slots
    first = read(slot)
    assert seconds(first) == 3600, first
    with playing(player, project, resources, profile, output / "reopen", 0) as (click, press, type_text, screenshot):
        click("timer-stats")
        click("calendar-edit-time")
        screenshot("restored.png")
        click("calendar-edit-minutes")
        press(269)
        press(259)
        type_text("2")
    second = read(slot)
    assert seconds(second) == 3720, second
    committed = slot.read_bytes()
    with playing(player, project, resources, profile, output / "invalid-input", 0) as (click, press, type_text, _):
        click("timer-stats")
        click("calendar-edit-time")
        click("calendar-edit-seconds")
        press(269)
        type_text("x")
    assert slot.read_bytes() == committed, "Exit committed nonnumeric work-time input"
    for phase, future in (
        ("future-payload", b'{ "schemaVersion": 1, "payload": [3, ["future work data"]] }'),
        ("future-envelope", b'{ "schemaVersion": 2, "payload": ["future work envelope"] }'),
    ):
        slot.write_bytes(committed)
        with playing(player, project, resources, profile, output / phase, 0) as (click, press, type_text, _):
            click("timer-stats")
            click("calendar-edit-time")
            click("calendar-edit-hours")
            press(269)
            press(259)
            type_text("3")
            slot.write_bytes(future)
        assert slot.read_bytes() == future, ("Exit replaced a newer work save", phase)
    for phase in phases:
        log = (output / phase / "stderr.log").read_text(encoding="utf-8")
        assert 'Insert instance layer "VK_LAYER_KHRONOS_validation"' in log, phase
        assert "VUID-" not in log and "SYNC-HAZARD" not in log, phase
    report = {
        "playerSHA256": digest(player), "assemblerSHA256": digest(Path(integrate_calendar.__code__.co_filename)),
        "fixtureSHA256": {p.relative_to(project.parent).as_posix(): digest(p)
                          for p in sorted(project.parent.rglob("*")) if p.is_file() and ".azure" not in p.parts},
        "firstExit": first, "secondExit": second, "invalidInputPreserved": True,
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
