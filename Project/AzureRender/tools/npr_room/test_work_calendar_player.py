"""Verify timer work accounting and reload in independent Player processes."""

import argparse
import json
from pathlib import Path

from test_notes_player import playing
from test_room_save_player import run_player


def work_slot(profile):
    slot, = profile.glob("project-*/save-work-calendar.json")
    return slot


def seconds(profile):
    document = json.loads(work_slot(profile).read_bytes())
    assert document["schemaVersion"] == 1 and document["payload"][0] == 1
    return sum(seconds for _, year in document["payload"][1] for _, seconds in year)


def replay(player, project, resource_root, output):
    output.mkdir()
    profile = output / "profile"
    actions = [(9, 84), (139, 84), (215, 84), (285, 82)]
    finished = run_player(player, project, profile, output / "pomodoro", (), resource_root,
                          keyframes=actions, frames=640)
    assert finished["audioStarts"] == 4
    pomodoro = seconds(profile)
    assert abs(pomodoro - (128 + 68) / 60) < .08, pomodoro

    countup = run_player(player, project, profile, output / "countup", (), resource_root,
                         keyframes=[(4, 77), (9, 84)], frames=390)
    assert countup["audioStarts"] == 2
    before_reload = work_slot(profile).read_bytes()
    total = seconds(profile)
    saved_session, = profile.glob("project-*/save-countup-session.json")
    elapsed = json.loads(saved_session.read_bytes())["payload"][1]
    assert elapsed >= 2 and elapsed <= total - pomodoro < elapsed + 1.01, (elapsed, total)

    with playing(player, project, resource_root, profile, output / "reopen", 0) as (click, _key, _text, screenshot):
        click("timer-stats")
        click("todo-toggle")
        screenshot()
        assert work_slot(profile).read_bytes() == before_reload
    assert work_slot(profile).read_bytes() == before_reload
    assert json.loads(saved_session.read_bytes())["payload"] == [1, 0, elapsed]
    report = {"pomodoro": pomodoro, "countupRecovered": elapsed, "total": total, "independentPlayers": 3}
    (output / "work-calendar-replay.json").write_text(json.dumps(report), encoding="utf-8")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("player", "project", "resource-root", "output"):
        parser.add_argument("--" + argument, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(replay(args.player.resolve(), args.project.resolve(), args.resource_root.resolve(),
                            args.output.resolve())))


if __name__ == "__main__":
    main()
