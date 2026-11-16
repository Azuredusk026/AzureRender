"""Verify concurrent room ambience, gain changes, and independent restart."""

import argparse
import json
from pathlib import Path

from test_notes_player import playing


def preferences(profile):
    slot, = profile.glob("project-*/save-ambient.json")
    document = json.loads(slot.read_bytes())
    assert document["schemaVersion"] == 1
    return document["payload"]


def opened_ambient(directory):
    lines = (directory / "stderr.log").read_text(encoding="utf-8").splitlines()
    return [Path(line.rsplit(" source: ", 1)[1]).stem
            for line in lines if " source: " in line and "assets\\ambient\\" in line]


def replay(player, project, resources, output):
    output.mkdir()
    profile = output / "profile"
    receipt = json.loads(project.with_name("ambient-receipt.json").read_bytes())
    ids = {row["id"]: row["uuid"] for row in receipt["sounds"]}
    assert len(ids) == 30 and sum(row["loop"] for row in receipt["sounds"]) == 28
    with playing(player, project, resources, profile, output / "write", 3) as (click, _, _, screenshot):
        click("ambient-toggle")
        click("ambient-select-8")
        click("ambient-mute")
        assert preferences(profile) == [1, [[8, 50, False]]]
        click("ambient-quiet")
        click("ambient-mute")
        assert preferences(profile) == [1, [[8, 45, True]]]
        click("ambient-mute")
        for _ in range(3):
            click("ambient-page-next")
        click("ambient-select-25")
        click("ambient-mute")
        assert preferences(profile) == [1, [[8, 45, False], [25, 50, False]]]
        screenshot()
    assert opened_ambient(output / "write") == [ids[8], ids[25]]

    with playing(player, project, resources, profile, output / "reopen", 3) as (click, _, _, _):
        click("ambient-toggle")
        click("ambient-select-8")
        click("ambient-mute")
        assert preferences(profile) == [1, [[8, 45, True], [25, 50, False]]]
    assert sorted(opened_ambient(output / "reopen")) == sorted((ids[8], ids[25]))
    for uuid in ids.values():
        assert not list((project.parent / ".azure/cache" / uuid).glob("*.azcache"))
    result = {"independentPlayers": 2, "audioStarts": [3, 3], "ambientSources": [ids[8], ids[25]],
              "preferences": preferences(profile)}
    (output / "ambient-replay.json").write_text(json.dumps(result), encoding="utf-8")
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("player", "project", "resource-root", "output"):
        parser.add_argument("--" + argument, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(replay(args.player.resolve(), args.project.resolve(), args.resource_root.resolve(),
                            args.output.resolve())))
