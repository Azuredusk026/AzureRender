"""Switch every original Windows language through the native room UI."""

import argparse
import json
from pathlib import Path
import time

import numpy as np
from PIL import Image

from reference_inventory import digest
from test_choice_room_player import checkpoint
from test_notes_player import playing
from test_room_voice_player import opened_voice


def language_save(profile):
    files = list(profile.glob("project-*/save-room-language.json"))
    if files:
        file, = files
        document = json.loads(file.read_bytes())
        assert document["schemaVersion"] == 1
        return document["payload"]
    return None


def expect_language(profile, index):
    deadline = time.monotonic() + 3
    observed = language_save(profile)
    while observed != [1, index]:
        assert time.monotonic() < deadline, (index, observed)
        time.sleep(.025)
        observed = language_save(profile)


def timer_region(path):
    with Image.open(path) as frame:
        assert frame.size == (1280, 720)
        return np.asarray(frame.crop((950, 28, 1263, 143)).convert("RGB"))


def replay(player, project, resources, output):
    output.mkdir()
    profile = output / "profile"
    first, reopened = output / "switch", output / "restore"
    data = json.loads(project.with_name("language-receipt.json").read_bytes())
    expected = ["Ja", "ZhHant", "Pt", "Ko", "ZhHans", "En", "Ru"]
    assert data["default"] == data["languages"]["ZhHans"]
    assert set(data["languages"]) == set(expected)
    assert set(data["fonts"]) == {"Japanese", "Traditional Chinese", "Korean"}
    for font in data["fonts"].values():
        assert digest(project.parent / "assets" / font["asset"]) == font["sha256"]
    korean_frame = first / "korean.png"
    saved = []
    with playing(player, project, resources, profile, first, 2) as (click, _, _, screenshot):
        click("room-language-toggle")
        for language in expected[:-1]:
            index = data["languages"][language]
            click(f"room-language-{index}")
            try:
                expect_language(profile, index)
            except AssertionError:
                screenshot()
                raise
            saved.append(index)
            if language == "Ko":
                screenshot()
                (first / "notes.png").rename(korean_frame)
        screenshot()
        click("room-voice-talk")
        deadline = time.monotonic() + 2
        while not opened_voice(first):
            assert time.monotonic() < deadline, "The original idle line did not begin"
            time.sleep(.02)
        voice, = opened_voice(first)
        index = data["languages"]["Ru"]
        click(f"room-language-{index}")
        expect_language(profile, index)
        assert opened_voice(first) == [voice]
        saved.append(index)

    episode_number, option = 11, 2
    engine_profile, = profile.glob("project-*")
    saved_choice = engine_profile / "save-work-conversation.json"
    assert not saved_choice.exists()
    saved_choice.write_text(json.dumps({"schemaVersion": 1, "payload": [1, episode_number, 2, 0]}),
                            encoding="utf-8")
    story = json.loads(project.with_name("choice-receipt.json").read_bytes())["episodes"][episode_number - 1]
    with playing(player, project, resources, profile, reopened, 2) as (click, _, _, screenshot):
        assert language_save(profile) == [1, data["languages"]["Ru"]]
        assert checkpoint(profile, 2) == [1, episode_number, 2, 0]
        screenshot()
        click(f"room-choice-option-{option}")
        assert checkpoint(profile, 3) == [1, episode_number, 3, option]
        assert checkpoint(profile, 0) == [1, 0, 0, 0]
    branch_audio = Path(story["options"][option - 1]["response"]["asset"]).name
    assert opened_voice(reopened) == [branch_audio]

    first_frame, restored_frame = first / "notes.png", reopened / "notes.png"
    changed = int(np.count_nonzero(np.any(timer_region(first_frame) != timer_region(restored_frame), axis=2)))
    assert changed > 100
    result = {"languages": expected, "persistedIds": saved, "independentPlayers": 2,
              "originalVoiceContinued": voice, "timerRegionChangedPixels": changed,
              "reopenedChoice": {"group": story["group"], "option": option, "voice": branch_audio},
              "screenshots": {"En": digest(first_frame), "Ko": digest(korean_frame), "Ru": digest(restored_frame)}}
    (output / "language-replay.json").write_text(json.dumps(result), encoding="utf-8")
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("player", "project", "resource-root", "output"):
        parser.add_argument("--" + argument, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(replay(args.player.resolve(), args.project.resolve(), args.resource_root.resolve(),
                            args.output.resolve())))
