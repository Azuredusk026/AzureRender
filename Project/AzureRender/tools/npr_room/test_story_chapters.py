"""Check the chapter data consumed by the room story reader."""

import unittest

from tools.npr_room.build_voice_room import LANGUAGES
from tools.npr_room.story_chapters import compile_chapters


class StoryChaptersTests(unittest.TestCase):
    def setUp(self):
        self.guid = "cae707ef2f774541a7eae7454f9cbdf4"
        self.checksum = "1" * 64
        self.media = {self.guid: {"source": "Assets/voice.ogg", "sha256": self.checksum}}
        self.native = {self.checksum + ".ogg": {"frames": 96000, "sampleRate": 48000}}

        def row(key, value, localization="0", arg1="0", arg2="0", arg3="0"):
            return {"ID": key, "ScenarioGroupID": "tutorial", "Command": str(value), "Arg1": arg1,
                    "Arg2": arg2, "Arg3": arg3, "Arg4": "0", "LocalizationID": localization,
                    "BodyMotion": "12", "FacialMotion": "15", "Talker": "1", "LookScale": "0.4",
                    "LookSpeedSeconds": "0.8", "LookEaseType": "0"}

        def text(key, voice):
            return {"ID": key, "Voice": voice, **{field: key + "-" + field for field in LANGUAGES}}

        self.story = {"groups": [{"ID": "tutorial", "Scenario": "18", "EpisodeNumber": "1", "UnlockLevel": "0",
                                  "TitleLocalizationID": "title", "SubtitleLocalizationID": "subtitle"}],
                      "commands": [row("duplicate", 100, arg1="1.5"), row("duplicate", 215, arg1=".05", arg2=".1"),
                                   row("options", 15, "option", "reply", "1"),
                                   row("reply", 0, "line", "1.5", "1.25"), row("end", 12)],
                      "commandsByGroup": {"tutorial": [0, 1, 2, 3, 4]},
                      "localization": [text(key, "voice" if key == "line" else "")
                                       for key in ("title", "subtitle", "option", "line")]}
        self.names = {0: "Text", 12: "EndScenario", 15: "TutorialSelection", 100: "SetDirectionDelaySeconds",
                      215: "DirectionMonochromeNoise"}

    def compile(self):
        return compile_chapters(self.story, self.names, self.media, self.native, ["tutorial"])

    def test_source_row_ids_and_typed_voice_timings(self):
        chapters, assets = self.compile()
        chapter, = chapters
        self.assertEqual(chapter["group"], "tutorial")
        self.assertEqual(chapter["category"], 18)
        self.assertEqual(chapter["unlockLevel"], 0)
        self.assertEqual(chapter["title"]["Ko"], "title-Ko")
        delay, noise, option, reply, end = chapter["commands"]
        self.assertEqual((delay["row"], noise["row"]), (0, 1))
        self.assertEqual(noise["command"], "DirectionMonochromeNoise")
        self.assertEqual(noise["arguments"], [".05", ".1", "0", "0"])
        self.assertEqual((option["target"], option["usedKey"], option["translations"]["Ja"]), (4, 1, "option-Ja"))
        self.assertEqual((reply["delay"], reply["hold"], reply["frames"], reply["sampleRate"]), (1.5, 1.25, 96000, 48000))
        self.assertEqual((reply["bodyMotion"], reply["faceMotion"], reply["look"]),
                         (12, 15, {"scale": .4, "seconds": .8, "ease": 0}))
        self.assertEqual(reply["audioGuid"], self.guid)
        self.assertEqual(assets[0]["sha256"], self.checksum)
        self.assertEqual(end["command"], "EndScenario")

    def test_branch_target_must_stay_in_the_group(self):
        self.story["commands"][2]["Arg1"] = "missing"
        with self.assertRaisesRegex(ValueError, "target"):
            self.compile()

    def test_text_can_have_an_empty_source_voice(self):
        self.story["localization"][-1]["Voice"] = ""
        chapters, assets = self.compile()
        self.assertEqual(assets, [])
        self.assertEqual(chapters[0]["commands"][3]["translations"]["ZhHans"], "line-ZhHans")
        self.assertNotIn("asset", chapters[0]["commands"][3])


if __name__ == "__main__":
    unittest.main()
