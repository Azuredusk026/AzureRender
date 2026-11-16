"""Validate Windows language identities and the translated timer inputs."""

from pathlib import Path
import tempfile
import unittest

from tools.npr_room.build_voice_room import localized_text_config
from tools.npr_room.room_language import FIELDS, LOCALES, UI_KEYS, language_config, read_locale_ids, ui_strings


class RoomLanguageTests(unittest.TestCase):
    def test_source_identity_must_match_the_original_enum_numbers(self):
        data = "None = 0,\n" + "\n".join(f"{name} = {index}," for index, (name, _, _) in enumerate(LOCALES, 1))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "GameLanguageType.cs"
            path.write_text(data, encoding="utf-8")
            self.assertEqual(read_locale_ids(path)["Portuguese"], 5)
            path.write_text(data.replace("Portuguese = 5", "Portuguese = 8"), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "selection IDs"):
                read_locale_ids(path)

    def test_missing_or_duplicate_ui_text_is_rejected(self):
        source = [{"ID": key, **{field: field for field in FIELDS}} for key in UI_KEYS.values()]
        localized = ui_strings(source)
        self.assertIn('language = {"Ja", "En", "ZhHans", "ZhHant", "Pt", "Ko", "Ru"}', language_config(localized, 3))
        with self.assertRaisesRegex(ValueError, "Duplicate"):
            ui_strings([*source, source[0]])
        with self.assertRaisesRegex(ValueError, "Missing UI text"):
            ui_strings(source[1:])
        source[0]["Pt"] = ""
        with self.assertRaisesRegex(ValueError, "Missing translation"):
            ui_strings(source)

    def test_unicode_and_control_characters_remain_valid_lua_literals(self):
        self.assertEqual(localized_text_config({"Ru": 'Русский\n"\\'}, ("Ru",)), r'{"Русский\010\"\\"}')


if __name__ == "__main__":
    unittest.main()
