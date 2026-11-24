"""Record the timed option prompt, cold resume, and branch-specific source voices."""

import argparse
import json
from pathlib import Path
import shutil
import time

from test_notes_player import playing
from test_room_voice_player import opened_voice


def checkpoint(profile, stage, timeout=20):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        paths = list(profile.glob("project-*/save-work-conversation.json"))
        if paths:
            path, = paths
            document = json.loads(path.read_bytes())
            assert document["schemaVersion"] == 1
            row = document["payload"]
            if row[2] == stage:
                return row
        time.sleep(.05)
    raise AssertionError(f"Work conversation did not reach stage {stage} in {profile}")


def replay(player, project, resources, output):
    output.mkdir()
    profile = output / "profile"
    receipt = json.loads(project.with_name("choice-receipt.json").read_bytes())
    assert len(receipt["episodes"]) == 20 and len(receipt["media"]) == 80
    with playing(player, project, resources, profile, output / "prompt", 4) as (click, _, _, screenshot):
        click("timer-toggle-button")
        time.sleep(2)
        click("timer-end-button")
        click("room-voice-talk")
        prompt = checkpoint(profile, 2)
        assert prompt[0] == 1 and prompt[3] == 0
        screenshot()
    episode = receipt["episodes"][prompt[1] - 1]
    assert opened_voice(output / "prompt") == [Path(episode["intro"]["asset"]).name]
    alternate_profile = output / "alternate-profile"
    shutil.copytree(profile, alternate_profile)

    observed = []
    for index, saved, name in ((2, profile, "first-response"), (3, alternate_profile, "alternate-response")):
        with playing(player, project, resources, saved, output / name, 2) as (click, _, _, _):
            assert checkpoint(saved, 2) == prompt
            click(f"room-choice-option-{index}")
            assert checkpoint(saved, 3) == [1, prompt[1], 3, index]
            assert checkpoint(saved, 0) == [1, 0, 0, 0]
        row = episode["options"][index - 1]
        assert opened_voice(output / name) == [Path(row["response"]["asset"]).name]
        observed.append({"option": index, "voice": row["response"]["audioGuid"], "endRow": row["endRow"]})

    report = {"independentPlayers": 3, "sourceEpisodes": len(receipt["episodes"]),
              "continuedEpisode": episode["group"], "branches": observed, "audioStarts": [4, 2, 2]}
    (output / "choice-replay.json").write_text(json.dumps(report), encoding="utf-8")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("player", "project", "resource-root", "output"):
        parser.add_argument("--" + argument, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(replay(args.player.resolve(), args.project.resolve(), args.resource_root.resolve(),
                            args.output.resolve())))
