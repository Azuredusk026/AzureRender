"""Verify source ordering, locked tracks and room playlist preferences."""

import json
import os
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest
from uuid import UUID

from tools.npr_room import build_music_room as music


class MusicPlaylistTests(unittest.TestCase):
    def test_original_order_and_typed_identity_select_only_unlocked_tracks(self):
        with TemporaryDirectory() as holder:
            root = Path(holder)
            source = root / "source"
            master = source / music.MUSIC_MASTER
            master.parent.mkdir(parents=True)
            table = []
            slots = {}
            for index, (label, locked, tag) in enumerate((("First", 0, 1), ("Locked", 1, 2),
                                                           ("Third <size=12>(Remix)</size>", 0, 4))):
                guid = UUID(int=index + 1).hex
                uuid = str(UUID(int=index + 4))
                name = f"Assets/track-{index}.ogg"
                clip = source / name
                clip.parent.mkdir(exist_ok=True)
                clip.write_bytes(label.encode())
                slots[guid] = {"source": name, "sha256": music.digest(clip)}
                table.extend((f"  - IsManualUnlock: {locked}", f"    Title: {label}", "    Credit: Composer",
                              f"    Tag: {tag}", f"    AudioClip: {{fileID: 8300000, guid: {guid}, type: 3}}",
                              f"    UUID: {uuid}"))
            master.write_text(
                "%YAML 1.1\n--- !u!114 &11400000\nMonoBehaviour:\n  AudioCollection:\n" + "\n".join(table),
                encoding="utf-8",
            )
            catalog = root / "audio-catalog.json"
            catalog.write_text(json.dumps({"slots": slots}), encoding="utf-8")
            audio_probe = root / "audio-probe.json"
            audio_probe.write_text(json.dumps({"files": {v["sha256"] + ".ogg": {"frames": 96000, "sampleRate": 48000}
                                                          for v in slots.values()}}), encoding="utf-8")
            playlist = music.music_catalog(source, catalog, audio_probe)
            rows = playlist["tracks"]
            self.assertEqual([row["title"] for row in rows],
                             ["First", "Locked", "Third <size=12>(Remix)</size>"])
            self.assertEqual([row["title"] for row in rows if not row["locked"]],
                             ["First", "Third <size=12>(Remix)</size>"])
            self.assertEqual(rows[2]["displayTitle"], "Third (Remix)")
            self.assertEqual(rows[1]["guid"], UUID(int=2).hex)
            self.assertEqual(rows[2]["uuid"], str(UUID(int=6)))
            self.assertEqual(rows[2]["tag"], 4)
            output = root / "assets"
            output.mkdir()
            music.stage_live_audio_media(source, rows, output)
            self.assertTrue(os.path.samefile(source / rows[2]["source"], output / rows[2]["asset"]))
            metadata = json.loads((output / rows[2]["asset"]).with_suffix(".ogg.azmeta").read_bytes())
            self.assertEqual(metadata["id"], rows[2]["uuid"])
            self.assertFalse(metadata["settings"]["retainSourceSnapshot"])
            self.assertNotIn("Locked", music.music_config(playlist))
            self.assertIn("96000 / 48000", music.music_config(playlist))
            (source / rows[2]["source"]).write_bytes(b"Changed")
            with self.assertRaisesRegex(ValueError, "identity"):
                music.stage_live_audio_media(source, rows, output)


if __name__ == "__main__":
    unittest.main()
