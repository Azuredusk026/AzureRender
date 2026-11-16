"""Check the original idle response and exact localized voice identity."""

import unittest

from tools.npr_room.build_voice_room import LANGUAGES, idle_responses


class RoomVoiceTests(unittest.TestCase):
    def test_accepts_a_single_original_idle_line_and_seven_translations(self):
        source = "Assets/Audio/Voice_Cue.ogg"
        guid = "a28e2fcc75804cc796d54a2be5caa3df"
        sha = "a" * 64
        story = {
            "groups": [{"ID": "click_normal", "Scenario": "8", "EpisodeNumber": "1"}],
            "commands": [{"Command": "0", "LocalizationID": "greeting", "Arg1": "0", "Arg2": "1", "Arg3": "0",
                          "BodyMotion": "-1", "FacialMotion": "2"}],
            "commandsByGroup": {"click_normal": [0]},
            "localization": [{"ID": "greeting", "Voice": "Voice_Cue", **{language: language for language in LANGUAGES}}],
        }
        catalog = {guid: {"source": source, "sha256": sha}}
        native = {sha + ".ogg": {"frames": 86400, "sampleRate": 48000}}
        responses, media = idle_responses(story, catalog, native)
        self.assertEqual(responses[0]["translations"], {language: language for language in LANGUAGES})
        self.assertEqual(responses[0]["frames"], 86400)
        self.assertEqual(responses[0]["faceMotion"], 2)
        self.assertEqual(media[0]["guid"], guid)
        self.assertEqual(media[0]["source"], source)
        story["commands"][0]["Command"] = "22"
        with self.assertRaisesRegex(ValueError, "idle response command"):
            idle_responses(story, catalog, native)


if __name__ == "__main__":
    unittest.main()
