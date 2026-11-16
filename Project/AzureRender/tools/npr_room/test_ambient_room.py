"""Check the typed environment audio identities and ordinary loop list."""

import json
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest
from uuid import UUID

from tools.npr_room.build_ambient_room import AMBIENT_ENUM, AMBIENT_MASTER, ambient_catalog, ambient_config


class AmbientRoomTests(unittest.TestCase):
    def test_stages_the_original_ids_and_separates_window_cues_from_loops(self):
        with TemporaryDirectory() as holder:
            root = Path(holder)
            source = root / "source"
            master = source / AMBIENT_MASTER
            master.parent.mkdir(parents=True)
            enum = source / AMBIENT_ENUM
            enum.parent.mkdir(parents=True)
            enum.write_text("public enum AmbientSoundType : byte {\n"
                            "    Fireworks_First = 5,\n    LightRain = 8\n}\n", encoding="utf-8")
            rows = []
            entries = {}
            for index, name in ((5, "firework"), (8, "rain")):
                guid = UUID(int=index).hex
                entries[guid] = {"source": f"Assets/{name}.ogg", "sha256": f"{index:064x}"}
                rows.append(f"  - AmbientSound: {index}\n"
                            f"    AudioClip: {{fileID: 8300000, guid: {guid}, type: 3}}\n")
            master.write_text("%YAML 1.1\n--- !u!114 &11400000\nMonoBehaviour:\n  AmbientSounds:\n"
                              + "".join(rows), encoding="utf-8")
            catalog = root / "audio-catalog.json"
            catalog.write_text(json.dumps({"slots": entries}), encoding="utf-8")
            receipt = ambient_catalog(source, catalog)
            firework, rain = receipt["sounds"]
            self.assertEqual(firework["uuid"], str(UUID(int=5)))
            self.assertFalse(firework["loop"])
            self.assertEqual(rain["name"], "Light Rain")
            self.assertTrue(rain["loop"])
            self.assertIn("room-ambient-8", ambient_config(receipt))
            self.assertNotIn("room-ambient-5", ambient_config(receipt))


if __name__ == "__main__":
    unittest.main()
