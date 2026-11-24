"""Replay music controls through the production UI and an isolated save root."""

import argparse
import json
from pathlib import Path

from test_notes_player import playing


def preferences(profile):
    slot, = profile.glob("project-*/save-music.json")
    document = json.loads(slot.read_bytes())
    assert document["schemaVersion"] == 1
    return document["payload"]


def opened_tracks(directory):
    return [Path(line.split("Streaming source: ", 1)[1]).stem
            for line in (directory / "stderr.log").read_text(encoding="utf-8").splitlines()
            if "Streaming source:" in line]


def replay(player, project, resources, output):
    output.mkdir()
    profile = output / "profile"
    catalog = json.loads(project.with_name("music-receipt.json").read_bytes())
    tracks = [row["uuid"] for row in catalog["tracks"] if not row["locked"]]
    assert len(tracks) == 42 and len(catalog["tracks"]) == 43
    with playing(player, project, resources, profile, output / "write", 4) as (click, _, _, screenshot):
        click("music-toggle")
        click("music-play")
        assert preferences(profile) == [1, False, 50, False, False, False]
        click("music-quiet")
        click("music-quiet")
        click("music-play")
        click("music-next")
        click("music-previous")
        click("music-shuffle")
        assert preferences(profile) == [1, True, 40, False, True, False]
        click("music-list")
        click("music-page-next")
        click("music-page-prev")
        click("music-select-3")
        click("music-mute")
        click("music-repeat")
        assert preferences(profile) == [1, True, 40, True, False, True]
        screenshot()
        click("music-play")
        assert preferences(profile) == [1, False, 40, True, False, True]
    assert opened_tracks(output / "write") == [tracks[index] for index in (0, 1, 0, 2)]

    with playing(player, project, resources, profile, output / "reopen", 2) as (click, _, _, _):
        click("music-toggle")
        click("music-mute")
        assert preferences(profile) == [1, False, 40, False, False, True]
        click("music-play")
        click("music-previous")
        assert preferences(profile) == [1, True, 40, False, False, True]
    assert opened_tracks(output / "reopen") == [tracks[0], tracks[-1]]
    report = {"independentPlayers": 2, "originalTracks": len(tracks), "audioStarts": [4, 2],
              "manualTracks": opened_tracks(output / "write"), "reopenedTracks": opened_tracks(output / "reopen"),
              "preferences": preferences(profile)}
    (output / "music-replay.json").write_text(json.dumps(report), encoding="utf-8")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("player", "project", "resource-root", "output"):
        parser.add_argument("--" + argument, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(replay(args.player.resolve(), args.project.resolve(), args.resource_root.resolve(),
                            args.output.resolve())))
