"""Replay the calendar, language selector and task panel through the regular room."""

import argparse
from calendar import monthrange
from datetime import date, timedelta
import json
from pathlib import Path
from uuid import uuid4

from test_notes_player import playing
from test_room_window_player import physical_client_pixels
from test_story_player import slot
from test_story_signal_room_player import entry
from test_todo_player import stored
from test_unlock_room_player import save
from tools.npr_room.build_story_signal_room import EXAMPLE, GRAPH
from tools.npr_room.build_work_calendar import integrate_calendar
from tools.npr_room.reference_inventory import digest
from tools.npr_room.room_language import FIELDS, calendar_config, calendar_strings


OBSERVER = """
local makeCalendar = new_work_calendar
function new_work_calendar(translate)
    local workLabel
    local calendar = makeCalendar(function(value)
        local text = translate(value)
        if value == calendar_labels.workTime then workLabel = text end
        return text
    end)
    local update = calendar.update_month_view
    local lastDate, lastLabel, lastTime
    function calendar:update_month_view(languageChanged)
        local day, seconds = update(self, languageChanged)
        if day ~= lastDate or workLabel ~= lastLabel or seconds ~= lastTime then
            if _ENV.self:try_write_save("calendar-diagnostic-selection", {day, seconds, workLabel}) then
                lastDate, lastLabel, lastTime = day, workLabel, seconds
            end
        end
        return day, seconds
    end
    return calendar
end
"""


def replay(player, project, resources, inventory, output):
    output.mkdir()
    room = project.parent
    assets = room / "assets"
    descriptor = json.loads(project.read_bytes())
    graph = json.loads((assets / GRAPH).read_bytes())
    labels = calendar_strings(json.loads(inventory.read_bytes())["localization"])
    controller, markup = integrate_calendar(
        descriptor, (assets / "timer.rml").read_text(encoding="utf-8"),
        (assets / "pomodoro.lua").read_text(encoding="utf-8"), EXAMPLE, calendar_config(labels))

    def configure(fixture, scene, prefix):
        fixture["runtime"]["input"] = descriptor["runtime"]["input"]
        ui, = [node["components"]["azure.game-ui"]["data"] for node in scene["nodes"]
               if node["components"].get("azure.game-ui", {}).get("data", {}).get("asset") == "assets:/timer.rml"]
        target = prefix + "timer.rml"
        ui["asset"] = "assets:/" + target
        return {target: markup}

    previous_results = []
    with entry(room, graph, controller + OBSERVER, additional_room=configure,
               tag="capture-calendar-room-" + uuid4().hex[:8]) as (fixture, level):
        for width, height in ((1280, 720), (960, 540)):
            profile = output / f"profile-{width}"
            current = date.today()
            previous = current.replace(day=1) - timedelta(days=1)
            first_weekday, month_days = monthrange(previous.year, previous.month)
            last_cell = (first_weekday + 1) % 7 + month_days
            work_days = {}
            for day, seconds in ((previous, 61), (current, 18)):
                work_days.setdefault(day.strftime("%Y"), []).append([day.strftime("%m%d"), seconds])
            work_data = [2, [[year, days] for year, days in work_days.items()], [2, 0, 0], 0]
            save(profile, fixture, "work-calendar", work_data)
            save(profile, fixture, "room-language", [1, 3])
            save(profile, fixture, "room-unlocks", [1, ["tutorial_01", "main_01"], [], []])
            sizes = []

            def ready(window, _):
                with physical_client_pixels():
                    assert window.find()
                    sizes.append(window.extent())

            with playing(player, fixture, resources, profile, output / f"client-{width}", level_reference=level,
                         on_ready=ready, player_args=("--width", str(width), "--height", str(height))) as (click, *_):
                time_labels = labels["ui_calendar_group_work_title"]
                assert sizes == [[width, height]], sizes
                click("timer-stats")
                assert slot(profile, "calendar-diagnostic-selection") == [
                    current.isoformat(), 18, time_labels["ZhHans"]]
                click("todo-toggle")
                click("calendar-prev-month")
                click(f"calendar-date-{last_cell}")
                click("todo-task-new")
                assert len(stored(profile)[2][0][2]) == 1
                click("room-language-toggle")
                observed_labels = {"ZhHans": time_labels["ZhHans"]}
                for index, locale in enumerate(FIELDS, start=1):
                    if locale == "ZhHans":
                        continue
                    click(f"room-language-{index}")
                    assert slot(profile, "room-language") == [1, index]
                    actual = slot(profile, "calendar-diagnostic-selection")
                    assert actual == [previous.isoformat(), 61, time_labels[locale]], actual
                    observed_labels[locale] = actual[2]
                click("timer-stats")
                click("room-language-3")
                click("timer-stats")
                assert slot(profile, "calendar-diagnostic-selection") == [
                    previous.isoformat(), 61, time_labels["ZhHans"]]
                click("room-language-toggle")
                click("calendar-next-month")
            assert slot(profile, "work-calendar") == work_data
            previous_results.append({"extent": sizes[0], "selection": actual, "calendarLabels": observed_labels})
    report = {"clientReplays": previous_results, "cleanExits": len(previous_results),
              "playerSHA256": digest(player), "sourceInventorySHA256": digest(inventory)}
    (output / "room-calendar-replay.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("player", "project", "resource-root", "inventory", "output"):
        parser.add_argument("--" + name, required=True, type=Path)
    options = parser.parse_args()
    print(json.dumps(replay(options.player, options.project, options.resource_root, options.inventory, options.output),
                     ensure_ascii=True))
