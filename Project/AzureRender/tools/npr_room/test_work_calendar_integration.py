"""Verify the calendar can be bound without replacing the assembled room UI."""

from pathlib import Path
import unittest

from tools.npr_room.build_work_calendar import integrate_calendar


class IntegratedCalendarTests(unittest.TestCase):
    def test_bind_calendar_preserves_room_controls(self):
        examples = Path(__file__).resolve().parents[2] / "examples/npr-room/assets"
        controller = (
            'local workSlot = "work-calendar"\nfunction new_work_calendar()\n'
            '    local calendar = {}\n    return calendar\nend\n\n'
            'local extraRoomModule = true\nworkCalendar = new_work_calendar()\n'
            '            local growth = workCalendar:progress()\n'
        )
        document = ('<style>\n#work-stats { display: block; margin-top: 8dp; font-size: 13dp; }\n'
                    '</style><span id="work-progression" style="display: none;"></span></div>'
                    '<button id="room-language-toggle"></button>')
        project = {"runtime": {"input": {"timer-stats": [], "room-language-toggle": []}}}
        script, markup = integrate_calendar(project, document, controller, examples)
        self.assertIn("local extraRoomModule = true", script)
        self.assertIn("workCalendar:update_month_view(languageChanged or ", script)
        self.assertIn('id="room-language-toggle"', markup)
        self.assertIn('id="calendar-next-month"', markup)
        self.assertIn('data-action="calendar-day-42"', markup)
        self.assertIn("calendar-day-42", project["runtime"]["input"])
        self.assertIn("room-language-toggle", project["runtime"]["input"])
