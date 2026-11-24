"""Replay count-up, pause, completion and restart in independent Players."""

import argparse
import json
from pathlib import Path

from test_room_save_player import run_player


def verify_countup(player, project, output, resource_root):
    output.mkdir()
    save_root = output / "profile"

    def read(slot):
        file, = save_root.glob(f"project-*/save-{slot}.json")
        document = json.loads(file.read_bytes())
        assert document["schemaVersion"] == 1
        return document["payload"]

    actions = [(4, 77), (9, 84), (139, 84), (215, 84), (285, 82)]
    complete = run_player(player, project, save_root, output / "complete", (), resource_root,
                          keyframes=actions, frames=520)
    assert complete["audioStarts"] == 5
    assert read("timer-mode") == [1, 1]
    assert read("countup-session") == [1, 0, 3]

    interrupted = run_player(player, project, save_root, output / "interrupted", [84], resource_root, frames=365)
    marker = read("countup-session")
    assert interrupted["audioStarts"] == 1 and marker[0] == 1 and marker[1] >= 2 and marker[2] == 3, marker
    recovered = run_player(player, project, save_root, output / "recovered", (), resource_root)
    assert recovered["audioStarts"] == 0 and read("timer-mode") == [1, 1]
    assert read("countup-session") == [1, 0, marker[1]]
    return {"processes": 3, "completedSeconds": 3, "restoredSeconds": marker[1],
            "audioStarts": [complete["audioStarts"], interrupted["audioStarts"], recovered["audioStarts"]]}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--player", type=Path, required=True)
    parser.add_argument("--project", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--resource-root", type=Path)
    args = parser.parse_args()
    print(json.dumps(verify_countup(args.player.resolve(), args.project.resolve(), args.output.resolve(),
                                     args.resource_root.resolve() if args.resource_root else None)))
