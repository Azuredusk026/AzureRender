"""Click room timer controls in the Player and verify their gameplay actions."""

import argparse
import json
import os
from pathlib import Path
import secrets
import struct
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from run_playable_long_run import NativeWindow
from test_validation_protocol import exchange


def verify_ui(player, project, output, resources, size=(960, 540)):
    output.mkdir()
    endpoint = output / "endpoint.json"
    save_root = output / "profile"
    token = secrets.token_hex(32)
    env = dict(os.environ, AZURE_TIMER_UI_TEST_TOKEN=token)
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    command = [
        str(player), "--project", str(project), "--resource-root", str(resources), "--save-root", str(save_root),
        "--width", str(size[0]), "--height", str(size[1]), "--validation-token-env", "AZURE_TIMER_UI_TEST_TOKEN",
        "--validation-endpoint", str(endpoint),
    ]
    snapshots = []
    with (output / "stdout.log").open("w", encoding="utf-8") as stdout, (output / "stderr.log").open("w", encoding="utf-8") as stderr:
        process = subprocess.Popen(command, cwd=output, env=env, stdout=stdout, stderr=stderr, startupinfo=startup)
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

            def wait(predicate):
                deadline = time.monotonic() + 40
                while process.poll() is None and time.monotonic() < deadline:
                    state = json.loads(call("query", name="game.state"))
                    assert not state["scriptErrors"] and not state["presentationErrors"] and not state["loadError"], state
                    if predicate(state):
                        snapshots.append(state)
                        return
                    time.sleep(.04)
                raise TimeoutError("Timer control did not produce the expected Player state")

            def click(target, audio_starts):
                reply = call("input", domain="game-ui", event={"action": "click", "target": target})
                assert reply["clicked"] == target
                wait(lambda state: state["audioStarts"] == audio_starts)

            wait(lambda state: state["levelReference"] == "assets:/room.azurelevel" and not state["loading"])
            click("timer-mode-button", 1)
            click("timer-toggle-button", 2)
            time.sleep(1.25)
            call("input", event={"action": "focus", "focused": True})
            call("input", event={"action": "key", "key": 84, "down": True})
            wait(lambda state: state["audioStarts"] == 3)
            call("input", event={"action": "key", "key": 84, "down": False})
            click("timer-toggle-button", 4)
            click("timer-end-button", 5)

            profile, = save_root.glob("project-*")
            mode = json.loads((profile / "save-timer-mode.json").read_bytes())
            work = json.loads((profile / "save-countup-session.json").read_bytes())
            assert mode == {"schemaVersion": 1, "payload": [1, 1]}, mode
            assert work["schemaVersion"] == 1 and work["payload"][:2] == [1, 0] and work["payload"][2] >= 1, work
            label = "npr-timer-" + secrets.token_hex(8)
            assert call("screenshot", label=label)["scheduled"]
            capture = resources / "captures" / (label + ".png")
            screenshot = output / "captures/timer-controls.png"
            deadline = time.monotonic() + 12
            while not capture.is_file():
                assert process.poll() is None and time.monotonic() < deadline
                time.sleep(.03)
            assert window.find()
            window.close()
            assert process.wait(timeout=45) == 0
            capture.rename(screenshot)
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=15)
    assert "Allocator after unload: buffers=0 images=0" in (output / "stdout.log").read_text(encoding="utf-8")
    with screenshot.open("rb") as stream:
        header = stream.read(24)
    assert header[:8] == b"\x89PNG\r\n\x1a\n" and struct.unpack(">II", header[16:24]) == size, header
    (output / "observations.json").write_text(json.dumps(snapshots), encoding="utf-8")
    return {"size": size, "uiClicks": 4, "keyboardPauses": 1, "audioStarts": 5, "savedWorkSeconds": work["payload"][2]}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("player", "project", "output", "resource-root"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    parser.add_argument("--size", type=int, nargs=2, default=(960, 540), metavar=("WIDTH", "HEIGHT"))
    args = parser.parse_args()
    print(json.dumps(verify_ui(args.player.resolve(), args.project.resolve(), args.output.resolve(),
                               args.resource_root.resolve(), tuple(args.size))))
