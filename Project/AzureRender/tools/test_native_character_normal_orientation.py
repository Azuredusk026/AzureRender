"""Normal orientation controls for imported and mirrored tangent frames."""
import math
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from PIL import Image

try:
    from normal_map_orientation import convert_normal_y
except ImportError:
    convert_normal_y = None


class NormalOrientationTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(convert_normal_y, 'Explicit normal Y conversion is missing')

    def test_reparameterized_frames_keep_the_same_world_normal(self):
        source = Image.new('RGBA', (1, 1), (181, 202, 232, 17))
        for angle in [0, .73]:
            tangent = [math.cos(angle), 0, math.sin(angle)]
            bitangent = [0, 1, 0]
            normal = [-math.sin(angle), 0, math.cos(angle)]
            target = convert_normal_y(source, 'positive', 'negative')
            a = [v/255*2-1 for v in source.getpixel((0, 0))[:3]]
            b = [v/255*2-1 for v in target.getpixel((0, 0))[:3]]
            for axis in range(3):
                expected = tangent[axis]*a[0]+bitangent[axis]*a[1]+normal[axis]*a[2]
                actual = tangent[axis]*b[0]-bitangent[axis]*b[1]+normal[axis]*b[2]
                self.assertAlmostEqual(actual, expected, places=14)

    def test_only_green_changes_and_the_input_is_retained(self):
        source = Image.new('RGBA', (2, 1))
        source.putdata([(19, 0, 253, 81), (87, 255, 44, 173)])
        before = source.tobytes()
        output = convert_normal_y(source, 'positive', 'negative')
        self.assertEqual([output.getpixel((x, 0)) for x in range(2)],
                         [(19, 255, 253, 81), (87, 0, 44, 173)])
        self.assertEqual(source.tobytes(), before)

    def test_rgb_round_trip_and_identity_preserve_the_payload(self):
        source = Image.new('RGB', (2, 1))
        source.putdata([(12, 127, 98), (45, 128, 207)])
        flipped = convert_normal_y(source, 'negative', 'positive')
        restored = convert_normal_y(flipped, 'positive', 'negative')
        self.assertEqual(restored.mode, 'RGB')
        self.assertEqual(restored.tobytes(), source.tobytes())
        identity = convert_normal_y(source, 'positive', 'positive')
        self.assertIsNot(identity, source)
        self.assertEqual(identity.tobytes(), source.tobytes())

    def test_rejects_ambiguous_channels_and_unknown_orientation(self):
        for mode in ['L', 'I', 'F']:
            with self.assertRaises(ValueError):
                convert_normal_y(Image.new(mode, (1, 1)), 'positive', 'negative')
        for source, target in [('unknown', 'negative'), ('positive', None), (True, 'negative')]:
            with self.assertRaises(ValueError):
                convert_normal_y(Image.new('RGB', (1, 1)), source, target)

    def test_cli_retains_existing_assets_and_provenance(self):
        with tempfile.TemporaryDirectory(prefix='azure-normal-y-') as directory:
            folder = Path(directory)
            source = folder/'source.png'
            Image.new('RGB', (1, 1), (31, 73, 191)).save(source)
            original = source.read_bytes()
            output = folder/'converted.png'
            provenance = output.with_suffix('.png.orientation.json')
            provenance.write_text('retained provenance\n')
            tool = Path(__file__).with_name('normal_map_orientation.py')
            command = [sys.executable, str(tool), '--input', str(source), '--output', str(output),
                       '--source-y', 'positive', '--target-y', 'negative']
            result = subprocess.run(command, capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse(output.exists())
            self.assertEqual(provenance.read_text(), 'retained provenance\n')
            self.assertEqual(source.read_bytes(), original)
            existing = folder/'existing.png'
            existing.write_bytes(original)
            command[command.index('--output')+1] = str(existing)
            result = subprocess.run(command, capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(existing.read_bytes(), original)


if __name__ == '__main__':
    unittest.main()
