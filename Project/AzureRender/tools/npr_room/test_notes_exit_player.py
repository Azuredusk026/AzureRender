"""Verify notebook exit recovery in a standalone project using the production Player."""

import argparse
from hashlib import sha256
import json
from pathlib import Path

from test_notes_player import playing, saved, saved_path


PANEL = """<rml>
<head><style>
body { margin: 32dp; font-family: LatoLatin; color: #f5e9d8; }
button { padding: 8dp; background-color: #34655d; }
#notes-panel { margin-top: 12dp; width: 390dp;
               padding: 14dp; background-color: #263836; }
#note-fields { display: block; margin-top: 8dp; }
input, textarea { display: block; width: 330dp; padding: 6dp;
                  margin-bottom: 9dp; background-color: #394f4b; }
textarea { height: 84dp; }
</style></head>
<body>
<button id="notes-toggle" data-action="notes-toggle">Open notebook</button>
<div id="notes-panel" style="display: none;">
  <span id="note-page"></span>
  <div id="note-fields">
    <input id="note-title" type="text" />
    <textarea id="note-text"></textarea>
    <button id="note-save" data-action="notes-save">Save page</button>
  </div>
  <span id="note-status"></span>
</div>
</body></rml>
"""


def make_fixture(root):
    assets = root / "assets"
    assets.mkdir(parents=True)
    project = root / "project.azureproject"
    project.write_text(json.dumps({
        "schemaVersion": 1, "id": "3decb065-ec9f-4669-bc3f-7517e17fb356", "name": "Notebook Exit",
        "mounts": [{"name": "assets", "path": "assets"}], "startupScene": "assets:/room.azurelevel",
        "runtime": {"schemaVersion": 1, "input": {"notes-toggle": [], "notes-save": []}, "systems": []},
    }), encoding="utf-8")
    level = {
        "schemaVersion": 1, "id": "room", "sceneType": "sample", "resources": [], "lights": [],
        "nodes": [{"id": "notebook", "components": {
            kind: {"type": kind, "version": 1, "data": {"asset": f"assets:/{name}"}}
            for kind, name in (("azure.script", "notes.lua"), ("azure.game-ui", "notebook.rml"))
        }}],
    }
    (assets / "room.azurelevel").write_text(json.dumps(level), encoding="utf-8")
    (assets / "notebook.rml").write_text(PANEL, encoding="utf-8")
    source = Path(__file__).resolve().parents[2] / "examples/npr-room/assets/notes.lua"
    (assets / "notes.lua").write_bytes(source.read_bytes())
    return project


def digest(path):
    return sha256(path.read_bytes()).hexdigest()


def verify(player, resources, output):
    project = make_fixture(output / "fixture")
    profile = output / "profile"
    shaders = {p.name: digest(p) for p in sorted((resources / "shaders").glob("*.spv"))}
    assert {"game-ui.vert.spv", "game-ui.frag.spv"} <= shaders.keys()
    with playing(player, project, resources, profile, output / "exit") as (click, _, type_text, screenshot):
        click("notes-toggle")
        blank = screenshot("blank.png")
        click("note-text")
        type_text("First line\nSecond line")
        draft = screenshot("draft.png")
        assert digest(blank) != digest(draft), "the draft must change the notebook image"
        assert not list(profile.glob("project-*/save-notes.json")), "draft saved before window close"

    assert list(profile.glob("project-*/save-notes.json")), "closing the Player did not save the open draft"
    first = saved(profile)
    assert first == [1, 2, [[1, "New Page", "First line\nSecond line"]]], first

    with playing(player, project, resources, profile, output / "reopen") as (click, press, type_text, screenshot):
        click("notes-toggle")
        restored = screenshot("restored.png")
        assert digest(restored) != digest(blank), "reopening must display the restored draft"
        click("note-text")
        press(269)
        type_text(" restored")
        click("note-save")
        reopened = saved(profile)[2][0][2]
        assert reopened == "First line\nSecond line restored", reopened
        path = saved_path(profile)
        future = b'{"schemaVersion":2,"payload":["preserve"]}'
        path.write_bytes(future)
        click("note-text")
        press(269)
        type_text(" rejected")

    assert saved_path(profile).read_bytes() == future, "exit overwrote a newer save envelope"
    for phase in ("exit", "reopen"):
        log = (output / phase / "stderr.log").read_text(encoding="utf-8", errors="replace")
        assert "VUID-" not in log and "SYNC-HAZARD" not in log, phase
    assert all(digest(resources / "shaders" / name) == value for name, value in shaders.items())
    inputs = (project, *(project.parent / "assets").iterdir())
    report = {
        "playerSHA256": digest(player),
        "fixtureSHA256": {p.relative_to(project.parent).as_posix(): digest(p) for p in sorted(inputs)},
        "shaderSHA256": shaders,
        "screenshotsSHA256": {p.name: digest(p) for p in (blank, draft, restored)},
        "restoredBody": reopened,
        "futureEnvelopePreserved": True,
        "liveAllocationsAfterExit": 0,
    }
    (output / "verification.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Notebook exit replay passed: {output / 'verification.json'}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--player", required=True, type=Path)
    parser.add_argument("--resource-root", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    verify(args.player.resolve(strict=True), args.resource_root.resolve(strict=True), args.output.resolve())
