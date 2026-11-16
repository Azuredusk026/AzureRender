"""Verify timer work accounting and reload in independent Player processes."""

import argparse
from datetime import date
import json
from pathlib import Path

from test_notes_player import playing
from test_room_save_player import run_player


def work_slot(profile):
    slot, = profile.glob("project-*/save-work-calendar.json")
    return slot


def work_state(profile):
    document = json.loads(work_slot(profile).read_bytes())
    assert document["schemaVersion"] == 1
    return document["payload"]


def seconds(state):
    return sum(seconds for _, year in state[1] for _, seconds in year)


def replay(player, project, resource_root, output, expect_progression=False):
    output.mkdir()
    profile = output / "profile"
    starting_seconds = 0
    if expect_progression:
        starting_seconds = 10
        project_id = json.loads(project.read_bytes())["id"]
        folder = profile / ("project-" + project_id.encode().hex())
        folder.mkdir(parents=True)
        today = date.today()
        (folder / "save-work-calendar.json").write_text(json.dumps({
            "schemaVersion": 1, "payload": [1, [[today.strftime("%Y"), [[today.strftime("%m%d"), starting_seconds]]]]],
        }), encoding="utf-8")
    actions = [(9, 84), (139, 84), (215, 84), (285, 82)]
    finished = run_player(player, project, profile, output / "pomodoro", (), resource_root,
                          keyframes=actions, frames=640)
    assert finished["audioStarts"] == 4
    first = work_state(profile)
    assert first[0] == (2 if expect_progression else 1)
    if expect_progression:
        assert first[2:] == [[2, 0, 0], 0], first
    pomodoro = seconds(first) - starting_seconds
    assert abs(pomodoro - (128 + 68) / 60) < .08, pomodoro

    countup = run_player(player, project, profile, output / "countup", (), resource_root,
                         keyframes=[(4, 77), (9, 84)], frames=390)
    assert countup["audioStarts"] == 2
    before_reload = work_slot(profile).read_bytes()
    interrupted = work_state(profile)
    total = seconds(interrupted)
    saved_session, = profile.glob("project-*/save-countup-session.json")
    elapsed = json.loads(saved_session.read_bytes())["payload"][1]
    active_seconds = total - starting_seconds - pomodoro
    assert elapsed >= 2 and elapsed <= active_seconds < elapsed + 1.01, (elapsed, total)
    if expect_progression:
        assert interrupted[2] == [2, 0, 0] and abs(interrupted[3] - active_seconds) < 1e-7

    with playing(player, project, resource_root, profile, output / "reopen", 0) as (click, _key, _text, screenshot):
        click("timer-stats")
        click("todo-toggle")
        screenshot()
    if expect_progression:
        recovered = work_state(profile)
        assert recovered[1] == interrupted[1] and recovered[3] == 0
        assert recovered[2][0] == 2 and recovered[2][2] == 0
        assert abs(recovered[2][1] - active_seconds / 6) < 1e-7, recovered
        after_reload = work_slot(profile).read_bytes()
        again = run_player(player, project, profile, output / "second-reopen", (), resource_root, frames=360)
        assert again["audioStarts"] == 0 and work_slot(profile).read_bytes() == after_reload
    else:
        assert work_slot(profile).read_bytes() == before_reload
    assert json.loads(saved_session.read_bytes())["payload"] == [1, 0, elapsed]
    report = {"pomodoro": pomodoro, "countupRecovered": elapsed, "total": total, "independentPlayers": 3}
    if expect_progression:
        report.update({"independentPlayers": 4, "progression": recovered[2], "legacyWorkSeconds": starting_seconds})
    (output / "work-calendar-replay.json").write_text(json.dumps(report), encoding="utf-8")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("player", "project", "resource-root", "output"):
        parser.add_argument("--" + argument, type=Path, required=True)
    parser.add_argument("--expect-progression", action="store_true")
    args = parser.parse_args()
    print(json.dumps(replay(args.player.resolve(), args.project.resolve(), args.resource_root.resolve(),
                            args.output.resolve(), args.expect_progression)))


if __name__ == "__main__":
    main()
