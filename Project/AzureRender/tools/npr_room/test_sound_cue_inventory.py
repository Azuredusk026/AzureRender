"""Check that room cues resolve to the clips selected by the original master tables."""

from hashlib import sha256
from pathlib import Path
from struct import pack
import tempfile
import unittest

from tools.npr_room.sound_cue_inventory import verify_media


class SoundCueTests(unittest.TestCase):
    def test_checks_direct_media_and_only_permits_repair_of_wave_lengths(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source.ogg"
            target = root / "target.ogg"
            source.write_bytes(b"Ogg source")
            target.write_bytes(source.read_bytes())
            self.assertNotIn("pcmSHA256", verify_media(source, target, {}))
            target.write_bytes(b"Different Ogg source")
            with self.assertRaisesRegex(ValueError, "identity"):
                verify_media(source, target, {})

            source = root / "source.wav"
            target = root / "target.wav"
            pcm = b"\x01\x00\x02\x00"
            original = b"RIFF" + pack("<I", 0) + b"WAVEfmt " + pack("<IHHIIHH", 16, 1, 1, 8000, 16000, 2, 16)
            original += b"data" + pack("<I", 0) + pcm
            fixed = original[:4] + pack("<I", len(original) - 8) + original[8:40] + pack("<I", len(pcm)) + pcm
            source.write_bytes(original)
            target.write_bytes(fixed)
            original_sha = sha256(original).hexdigest()
            repairs = {f"media/{original_sha}.wav": {
                "sourceSHA256": original_sha, "sha256": sha256(fixed).hexdigest(),
                "pcmSHA256": sha256(pcm).hexdigest(),
            }}
            self.assertEqual(verify_media(source, target, repairs)["pcmSHA256"], sha256(pcm).hexdigest())
            wrong = fixed[:-1] + b"\xff"
            target.write_bytes(wrong)
            repairs[f"media/{original_sha}.wav"]["sha256"] = sha256(wrong).hexdigest()
            with self.assertRaisesRegex(ValueError, "identity"):
                verify_media(source, target, repairs)
