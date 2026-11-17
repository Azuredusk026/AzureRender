from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

import numpy as np

from tools.npr_room.windows_noise_probe import compare_screen_move, screen_move_source, verify_screen_move


class ScreenMoveCaptureTest(unittest.TestCase):
    def test_verifies_displacement_without_recoloring(self):
        source = screen_move_source()
        frames = np.stack([source, np.roll(source, 2, axis=1), np.roll(source, 3, axis=1),
                           np.roll(source, 1, axis=0)])
        with TemporaryDirectory() as temporary:
            first = Path(temporary) / "first.rgba32f"
            repeat = Path(temporary) / "repeat.rgba32f"
            for path in (first, repeat):
                path.write_bytes(frames.tobytes())
            report = verify_screen_move(first, repeat)
            self.assertEqual(report["changedPixels"], [0, 57600, 57600, 57600])
            self.assertEqual(compare_screen_move(first, first, repeat)["matchingPixels"], 230400)
            other = Path(temporary) / "other.rgba32f"
            other.write_bytes(frames[[0, 2, 1, 3]].tobytes())
            with self.assertRaisesRegex(ValueError, "differs"):
                compare_screen_move(other, first, repeat)
            frames[1, 0, 0, 2] = 0.123
            for path in (first, repeat):
                path.write_bytes(frames.tobytes())
            with self.assertRaisesRegex(ValueError, "sampled source"):
                verify_screen_move(first, repeat)
