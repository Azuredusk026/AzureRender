"""Replay note editing and rollback using the production Player and a disposable save root."""

import argparse
import contextlib
import json
import os
from pathlib import Path
import secrets
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from run_playable_long_run import NativeWindow
from test_validation_protocol import exchange


@contextlib.contextmanager
def playing(player, project, resources, profile, output, expected_audio_starts=None, level_reference="assets:/room.azurelevel"):
    output.mkdir()
    endpoint = output / "endpoint.json"
    token = secrets.token_hex(32)
    env = dict(os.environ, AZURE_NOTES_TEST_TOKEN=token)
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    command = [
        str(player), "--project", str(project), "--resource-root", str(resources), "--save-root", str(profile),
        "--width", "1280", "--height", "720", "--validation-token-env", "AZURE_NOTES_TEST_TOKEN",
        "--validation-endpoint", str(endpoint),
    ]
    with (output / "stdout.log").open("w", encoding="utf-8") as out, (output / "stderr.log").open("w", encoding="utf-8") as err:
        process = subprocess.Popen(command, cwd=output, env=env, stdout=out, stderr=err, startupinfo=startup)
        window = NativeWindow(process.pid)
        try:
            deadline = time.monotonic() + 90
            while not endpoint.is_file():
                assert process.poll() is None and time.monotonic() < deadline
                time.sleep(.05)
            address = json.loads(endpoint.read_bytes())

            def call(op, **fields):
                reply = exchange(address, token, dict(op=op, **fields))
                assert reply["passed"], reply
                return reply.get("value")

            while True:
                assert process.poll() is None and time.monotonic() < deadline
                query = exchange(address, token, {"op": "query", "name": "game.state"})
                if query.get("code") == "Timeout" and not query["passed"]:
                    continue
                assert query["passed"], query
                state = json.loads(query["value"])
                assert not state["scriptErrors"] and not state["presentationErrors"], state
                if state["levelReference"] == level_reference and not state["loading"]:
                    break
                time.sleep(.05)
            call("wait-frames", frames=5)

            def click(target):
                response = call("input", domain="game-ui", event={"action": "click", "target": target})
                assert response["clicked"] == target
                call("wait-frames", frames=4)
                check_state()

            def check_state():
                state = json.loads(call("query", name="game.state"))
                assert not state["scriptErrors"] and not state["presentationErrors"] and not state["loadError"], state
                return state

            def press(key):
                for down in (True, False):
                    call("input", domain="game-ui", event={"action": "key", "key": key, "down": down})

            def type_text(value):
                for character in value:
                    if character == "\n":
                        press(257)
                    else:
                        call("input", domain="game-ui", event={"action": "character", "value": ord(character)})

            def screenshot():
                label = "npr-notes-" + secrets.token_hex(8)
                assert call("screenshot", label=label)["scheduled"]
                capture = resources / "captures" / (label + ".png")
                deadline = time.monotonic() + 12
                while not capture.is_file():
                    assert process.poll() is None and time.monotonic() < deadline
                    time.sleep(.03)
                call("wait-frames", frames=1)
                capture.rename(output / "notes.png")

            yield click, press, type_text, screenshot
            state = check_state()
            if expected_audio_starts is not None:
                assert state["audioStarts"] == expected_audio_starts, state
            assert window.find()
            window.close()
            assert process.wait(timeout=45) == 0
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=15)
    assert "Allocator after unload: buffers=0 images=0" in (output / "stdout.log").read_text(encoding="utf-8")


def saved_path(profile):
    path, = profile.glob("project-*/save-notes.json")
    return path


def saved(profile):
    path = saved_path(profile)
    value = json.loads(path.read_bytes())
    assert value["schemaVersion"] == 1
    return value["payload"]


def verify_notes(player, project, resources, output):
    output.mkdir()
    profile = output / "profile"
    with playing(player, project, resources, profile, output / "write") as (click, press, type_text, screenshot):
        click("notes-toggle")
        click("note-title")
        press(269)
        for _ in "New Page":
            press(259)
        type_text("项目记录")
        click("note-text")
        type_text("第一行\n<记录>")
        click("note-save")
        first = saved(profile)
        assert first == [1, 2, [[1, "项目记录", "第一行\n<记录>"]]], first
        screenshot()
        click("note-new")
        click("note-text")
        type_text("Second entry")
        click("note-up")
        assert [row[0] for row in saved(profile)[2]] == [2, 1]
        click("note-next")
        click("note-delete")
        assert saved(profile) == [1, 3, [[2, "New Page", "Second entry"]]], saved(profile)
        click("notes-toggle")

    with playing(player, project, resources, profile, output / "reopen") as (click, press, type_text, _):
        click("notes-toggle")
        click("note-text")
        press(269)
        type_text(" restored")
        path = saved_path(profile)
        original = path.read_bytes()
        future = b'{"schemaVersion":2,"payload":["preserve"]}'
        path.write_bytes(future)
        click("note-save")
        assert path.read_bytes() == future
        path.write_bytes(original)
        click("note-save")
        page = saved(profile)[2][0]
        assert page == [2, "New Page", "Second entry restored"], page
        click("note-delete")
        assert saved(profile)[2] == [page]

    report = {"independentPlayers": 2, "multilineUnicode": first[2][0], "remainingPage": page,
              "reordered": True, "deleted": True, "keptLastPage": True, "rejectedFutureVersionAndRetried": True}
    (output / "notes-replay.json").write_text(json.dumps(report, ensure_ascii=False), encoding="utf-8")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--player", type=Path, required=True)
    parser.add_argument("--project", type=Path, required=True)
    parser.add_argument("--resource-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(verify_notes(args.player.resolve(), args.project.resolve(), args.resource_root.resolve(),
                                  args.output.resolve()), ensure_ascii=False))


if __name__ == "__main__":
    main()
