"""Check the source timing fields and alternate timer mode identities."""

import unittest

from tools.npr_room.build_voice_room import LANGUAGES
from tools.npr_room.timer_voice import timer_voice_rows
from tools.npr_room.timer_voice_motion import resolve_motions


class TimerVoiceTests(unittest.TestCase):
    def test_native_clip_duration_can_include_one_terminal_sample(self):
        source = {"55": {"clips": [{"clip": "Assets/hello.anim", "speed": 1, "loop": False,
                                   "sha256": "proof", "start": 0, "stop": 4,
                                   "voiceEvents": [{"cue": 1, "seconds": 1.2}]}]}}
        clip = {"name": "hello", "human": True, "rate": 60, "length": 4 + 1 / 60, "frames": [0]}
        motion = resolve_motions(source, [clip], [55])["55"]
        self.assertEqual(motion["seconds"], clip["length"])
        self.assertEqual(motion["serializedSeconds"], 4)
        self.assertEqual(motion["cues"], {"1": 1.2})
        clip["length"] += 1 / 60
        with self.assertRaisesRegex(ValueError, "time range"):
            resolve_motions(source, [clip], [55])

    def test_motion_wait_and_mouth_control_are_not_text_delays(self):
        guid = "cacf9d894afe37e4f98c9f4d9f16167d"
        checksum = "1" * 64
        localization = {"ID": "work-line", "Voice": "start", **{language: language for language in LANGUAGES}}
        command = {"Command": "22", "LocalizationID": "work-line", "Arg1": "2", "Arg2": "-1", "Arg3": "-1",
                   "BodyMotion": "55", "FacialMotion": "7", "LookScale": "0.5",
                   "LookSpeedSeconds": "0.75", "LookEaseType": "0"}
        story = {
            "groups": [{"ID": "first", "Scenario": "4", "EpisodeNumber": "1"},
                       {"ID": "stopwatch", "Scenario": "3000", "EpisodeNumber": "1"}],
            "commandsByGroup": {"first": [0], "stopwatch": [1]},
            "commands": [command, {**command, "BodyMotion": "-1", "Arg2": "0", "Arg3": "0"}],
            "localization": [localization],
        }
        media = {guid: {"source": "Assets/AudioClip/start.ogg", "sha256": checksum}}
        decoded = {checksum + ".ogg": {"frames": 96000, "sampleRate": 48000}}
        rows, sources = timer_voice_rows(story, media, decoded, {"4": 1, "3000": 1})
        first, second = rows["4"][0], rows["3000"][0]
        self.assertEqual(first["animationCue"], 2)
        self.assertTrue(first["speakBeforeMotion"])
        self.assertFalse(first["moveMouth"])
        self.assertEqual(first["look"], {"scale": .5, "seconds": .75, "ease": 0})
        self.assertEqual(first["frames"], 96000)
        self.assertEqual(second["audioGuid"], first["audioGuid"])
        self.assertTrue(second["moveMouth"])
        self.assertFalse(second["speakBeforeMotion"])
        self.assertEqual(len(sources), 1)
        with self.assertRaisesRegex(ValueError, "episode"):
            timer_voice_rows(story, media, decoded, {"4": 2, "3000": 1})


if __name__ == "__main__":
    unittest.main()
