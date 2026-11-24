"""Verify ordered room tasks, completion timestamps, and independent Player reload."""

import argparse
from datetime import datetime
import json
from pathlib import Path

from test_notes_player import playing


def stored(profile):
    slot, = profile.glob("project-*/save-todo.json")
    state = json.loads(slot.read_bytes())
    assert state["schemaVersion"] == 1
    return state["payload"]


def replay(player, project, resources, output):
    output.mkdir()
    profile = output / "profile"
    with playing(player, project, resources, profile, output / "write", 5) as (click, _, type_text, screenshot):
        def enter(control, text):
            click(control)
            type_text(text)

        click("todo-toggle")
        enter("todo-list-title", "项目")
        click("todo-task-new")
        enter("todo-task-text", "设计")
        enter("todo-task-due", "2028-02-29")
        click("todo-task-new")
        enter("todo-task-text", "复核")
        click("todo-task-up")
        assert [task[0] for task in stored(profile)[2][0][2]] == [3, 2]
        since = datetime.now().replace(microsecond=0)
        click("todo-task-complete")
        _, text, completion, _ = stored(profile)[2][0][2][0]
        stamp = datetime.strptime(completion, "%Y%m%d%H%M%S")
        assert text == "复核" and since <= stamp <= datetime.now()
        click("todo-list-new")
        enter("todo-list-title", "日程")
        click("todo-task-new")
        enter("todo-task-text", "会议")
        click("todo-list-prev")
        first = stored(profile)
        assert first == [1, 6, [[1, "项目", [[3, "复核", completion, ""], [2, "设计", "", "2028-02-29"]]],
                                [4, "日程", [[5, "会议", "", ""]]]]], first
        screenshot()

    with playing(player, project, resources, profile, output / "reopen", 3) as (click, _, type_text, screenshot):
        click("todo-toggle")
        click("todo-task-next")
        click("todo-task-complete")
        assert stored(profile)[2][0][2][0] == [3, "复核", "", ""]
        click("todo-task-delete")
        click("todo-list-next")
        click("todo-list-delete")
        click("todo-list-delete")
        restored = stored(profile)
        assert restored == [1, 6, [[1, "项目", [[2, "设计", "", "2028-02-29"]]]]], restored

    report = {"independentPlayers": 2, "recordedCompletion": completion, "restoredTask": restored[2][0][2][0],
              "reordered": True, "reopened": True, "removedList": True, "keptLastList": True, "audioStarts": [5, 3]}
    (output / "todo-replay.json").write_text(json.dumps(report, ensure_ascii=False), encoding="utf-8")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("player", "project", "resource-root", "output"):
        parser.add_argument("--" + argument, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(replay(args.player.resolve(), args.project.resolve(), args.resource_root.resolve(),
                            args.output.resolve()), ensure_ascii=False))


if __name__ == "__main__":
    main()
