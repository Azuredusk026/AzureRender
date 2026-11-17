import tempfile
import unittest
from pathlib import Path

from tools.npr_room.windows_sky_probe import COLOR_FIELDS, capture_globals


class WindowsSkyProbeTests(unittest.TestCase):
    def test_clock_and_color_profile_identify_the_original_draw(self):
        values = {field: dict(r=.25, g=.5, b=.75, a=1) for field in COLOR_FIELDS.values()}
        packed = " ".join(f"global:{binding}ID=RGBA(0.25,0.5,0.75,1)" for binding in COLOR_FIELDS)
        shader = "shader=Distant Lands/Cozy/URP/Stylized Sky Desktop"
        wrong_profile = packed.replace("CZY_FilterColorID=RGBA(0.25,0.5,0.75,1)", "CZY_FilterColorID=RGBA(1,1,1,1)")
        rows = [f"t=1.0 hours=18 minutes=30 {shader} {packed}",
                f"t=2.0 hours=12 minutes=30 {shader} {wrong_profile}",
                f"t=3.0 hours=12 minutes=30 {shader} {packed}"]
        with tempfile.TemporaryDirectory() as temporary:
            log = Path(temporary) / "probe.log"
            log.write_text("\n".join(rows), encoding="utf-8")
            seconds, fields = capture_globals(log, values, .52083330, "Day")
            self.assertEqual(seconds, 3)
            self.assertEqual(fields["CZY_FilterColor"], (.25, .5, .75, 1))
            changed = {**values, "filterColor": dict(r=.9, g=.1, b=.2, a=1)}
            with self.assertRaisesRegex(ValueError, "Cloudy"):
                capture_globals(log, changed, .52083330, "Cloudy")
