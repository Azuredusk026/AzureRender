"""Replay idle voice responses and preserve native source and caption evidence."""

import argparse
import json
from pathlib import Path
import time

from test_notes_player import playing


def opened_voice(directory):
    lines = (directory / "stderr.log").read_text(encoding="utf-8").splitlines()
    return [Path(line.rsplit(" source: ", 1)[1]).name
            for line in lines if " source: " in line and "assets\\voice\\" in line]


def replay(player, project, resources, output):
    output.mkdir()
    receipt = json.loads(project.with_name("voice-receipt.json").read_bytes())
    voices = {Path(row["asset"]).name: row for row in receipt["responses"]}
    assert len(voices) == 23 and receipt["language"] == "ZhHans"
    with playing(player, project, resources, output / "profile", output / "room", 3) as (click, _, _, screenshot):
        click("room-voice-talk")
        click("room-voice-talk")
        assert len(opened_voice(output / "room")) == 1
        screenshot()
        selected, = opened_voice(output / "room")
        line = voices[selected]
        assert line["translations"]["ZhHans"] and line["moveMouth"]
        time.sleep(line["frames"] / line["sampleRate"] + line["hold"] + .5)
        click("room-voice-talk")
        first, second = opened_voice(output / "room")
        assert first == selected and first != second and second in voices
    report = {"audioStarts": 3, "available": len(voices), "language": receipt["language"],
              "observed": [voices[name]["group"] for name in (first, second)]}
    (output / "voice-replay.json").write_text(json.dumps(report), encoding="utf-8")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("player", "project", "resource-root", "output"):
        parser.add_argument("--" + argument, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(replay(args.player.resolve(), args.project.resolve(), args.resource_root.resolve(),
                            args.output.resolve())))
