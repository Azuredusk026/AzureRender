"""Check ambient restrictions and restore original granted audio on cold start."""

import argparse
import json
from pathlib import Path

from test_ambient_room_player import opened_ambient, preferences
from test_notes_player import playing


def save(profile, project, slot, value):
    identity = json.loads(project.read_bytes())["id"]
    path = profile / ("project-" + identity.encode("utf-8").hex()) / f"save-{slot}.json"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps({"schemaVersion": 1, "payload": value}), encoding="utf-8")


def replay(player, project, resources, output):
    output.mkdir()
    profile = output / "profile"
    receipt = json.loads(project.with_name("ambient-receipt.json").read_bytes())
    ids = {row["id"]: row["uuid"] for row in receipt["sounds"]}
    save(profile, project, "ambient", [1, [[8, 45, False], [25, 50, False]]])
    with playing(player, project, resources, profile, output / "locked", 2) as (click, _, _, screenshot):
        click("ambient-toggle")
        click("ambient-select-8")
        screenshot()
        click("ambient-page-next")
        click("ambient-select-9")
        click("ambient-mute")
        assert preferences(profile) == [1, [[8, 45, False], [9, 50, False], [25, 50, False]]]
    assert opened_ambient(output / "locked") == [ids[9]]

    save(profile, project, "room-unlocks", [1, ["main_19"], ["City"], []])
    with playing(player, project, resources, profile, output / "granted", 4) as (click, _, _, _):
        click("ambient-toggle")
        click("ambient-select-8")
        click("ambient-mute")
        assert preferences(profile) == [1, [[8, 45, True], [9, 50, False], [25, 50, False]]]
    assert sorted(opened_ambient(output / "granted")) == sorted(ids[key] for key in (8, 9, 25))
    result = {"independentPlayers": 2, "lockedProfileAudio": [ids[9]],
              "grantedProfileAudio": [ids[key] for key in (8, 9, 25)], "audioStarts": [2, 4]}
    (output / "unlock-replay.json").write_text(json.dumps(result), encoding="utf-8")
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("player", "project", "resource-root", "output"):
        parser.add_argument("--" + argument, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(replay(args.player.resolve(), args.project.resolve(), args.resource_root.resolve(),
                            args.output.resolve())))
