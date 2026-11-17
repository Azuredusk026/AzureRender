from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

import numpy as np

from tools.npr_room.windows_noise_probe import BACKGROUND, compare_glitch, glitch_screen_source, verify_glitch


class GlitchCaptureTest(unittest.TestCase):
    def test_checks_timing_and_coverage_of_glitch(self):
        texture = glitch_screen_source()
        self.assertEqual((texture.shape, texture.dtype), ((288, 512, 4), np.dtype("uint8")))
        frames = np.broadcast_to(BACKGROUND, (4, 180, 320, 4)).copy()
        frames[1, 20:40, :, :3] = .3
        frames[2, 10:45, :, :3] = .6
        frames[3, 15:50, :, :3] = .4
        with TemporaryDirectory() as temporary:
            first = Path(temporary) / "first.rgba32f"
            repeat = Path(temporary) / "repeat.rgba32f"
            for path in (first, repeat):
                path.write_bytes(frames.tobytes())
            self.assertEqual(verify_glitch(first, repeat)["changedPixels"], [0, 6400, 11200, 11200])
            self.assertEqual(compare_glitch(first, first, repeat)["matchingPixels"], 230400)
            alternate = Path(temporary) / "alternate.rgba32f"
            alternate.write_bytes(frames[[0, 2, 1, 3]].tobytes())
            with self.assertRaisesRegex(ValueError, "differs"):
                compare_glitch(alternate, first, repeat)
            frames[0, 0, 0, 0] = .75
            for path in (first, repeat):
                path.write_bytes(frames.tobytes())
            with self.assertRaisesRegex(ValueError, "zero-ratio"):
                verify_glitch(first, repeat)
