from hashlib import sha256
from io import BytesIO
import json
from pathlib import Path
from struct import pack, pack_into
import tempfile
import unittest
import wave

from audio_repair import repair_wave_header, stage_audio_repairs


def recovery_wave():
    samples = pack("<8h", 0, 0, 14, -10, 28, -20, 26, -18)
    header = pack("<4sI4s4sIHHIIHH4sI", b"RIFF", 0, b"WAVE", b"fmt ", 16,
                  1, 2, 48000, 192000, 4, 16, b"data", 0)
    return header + samples


class AudioRepairTests(unittest.TestCase):
    def test_fills_only_empty_length_fields_and_preserves_pcm(self):
        source = recovery_wave()
        repaired = repair_wave_header(source)
        self.assertEqual(repaired[:4], source[:4])
        self.assertEqual(repaired[8:40], source[8:40])
        self.assertEqual(repaired[44:], source[44:])
        with wave.open(BytesIO(repaired)) as result:
            self.assertEqual((result.getnchannels(), result.getframerate(), result.getnframes()), (2, 48000, 4))
            self.assertEqual(result.readframes(4), source[44:])
        self.assertIsNone(repair_wave_header(repaired))
        with self.assertRaisesRegex(ValueError, "Invalid Recovery PCM layout"):
            repair_wave_header(source[:-1])
        partial = bytearray(source)
        pack_into("<I", partial, 4, len(source) - 8)
        with self.assertRaisesRegex(ValueError, "Both Recovery WAV lengths"):
            repair_wave_header(partial)

    def test_stages_a_distinct_replacement_for_one_shared_payload(self):
        data = recovery_wave()
        digest = sha256(data).hexdigest()
        slot = {"sha256": digest, "media": f"media/{digest}.wav"}
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "media").mkdir()
            source = root / slot["media"]
            source.write_bytes(data)
            manifest = root / "audio-catalog.json"
            manifest.write_text(json.dumps({"slots": {"one": slot, "two": slot}}), encoding="utf-8")
            output = root / "repaired"
            result = stage_audio_repairs(manifest, output)
            self.assertEqual(len(result["repairs"]), 1)
            entry = result["repairs"][slot["media"]]
            staged = (output / entry["media"]).read_bytes()
            self.assertEqual(entry["sha256"], sha256(staged).hexdigest())
            self.assertEqual(entry["pcmSHA256"], sha256(data[44:]).hexdigest())
            self.assertEqual(source.read_bytes(), data)


if __name__ == "__main__":
    unittest.main()
