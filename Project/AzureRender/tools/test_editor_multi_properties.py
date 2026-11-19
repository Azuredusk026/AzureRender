"""Check two-project mixed-value editing and captured reference targets."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import uuid

from create_playable_project import create
from editor_test_host import EditorHost


def component(kind, data):
    return {"type": kind, "version": 1, "data": data}


def key(host, value):
    host.call("input", event={"action": "key", "key": value, "down": True})
    host.frames()
    host.call("input", event={"action": "key", "key": value, "down": False})
    host.frames()


def check_drag_cancel(host, workflow):
    target = "axis.value.azure.transform.translation.0"
    before = host.query("document.contentHash")
    initial = host.workspace()
    def drag():
        host.call("input", event={"action": "mouse", "target": target,
                                  "down": True})
        host.frames()
        for offset in (25, 50):
            host.call("input", event={"action": "mouse", "target": target,
                                      "down": True, "offset": [offset, 0]})
            host.frames()

    def release():
        host.call("input", event={"action": "mouse", "target": target,
                                  "down": False, "offset": [50, 0]})
        host.frames()

    drag()
    assert host.query("document.contentHash") != before, "Numeric drag must actually edit"
    host.screenshot("drag-active-" + workflow)
    key(host, "Escape")
    release()
    host.screenshot("drag-cancelled-" + workflow)
    after = host.workspace()
    assert host.query("document.contentHash") == before, "Escape must restore original mixed values"
    assert after["selection"] == initial["selection"]
    assert after["undoCount"] == initial["undoCount"], "Cancelled drag must discard its undo entry"
    assert after["dirty"] == initial["dirty"]
    drag()
    release()
    committed = host.query("document.contentHash")
    assert committed != before
    assert host.workspace()["undoCount"] == initial["undoCount"] + 1
    host.edit("history.undo")
    assert host.query("document.contentHash") == before
    # This cancellation must retain the redo entry created by the normal drag.
    drag()
    key(host, "Escape")
    release()
    assert host.query("document.contentHash") == before
    assert host.workspace()["selection"] == initial["selection"]
    assert host.workspace()["undoCount"] == initial["undoCount"]
    host.edit("history.redo")
    assert host.query("document.contentHash") == committed
    host.edit("history.undo")
    assert host.query("document.contentHash") == before


def run_case(executable, output, workflow):
    project = output / "project"
    if workflow == "exploration":
        create(project)
    else:
        source = Path(__file__).resolve().parents[1] / "assets_public/scene_inspector"
        shutil.copytree(source, project, ignore=shutil.ignore_patterns(".azure"))
    project_file = project / "project.azureproject"
    descriptor = json.loads(project_file.read_bytes())
    level = project / "assets" / descriptor["startupScene"].removeprefix("assets:/")
    document = json.loads(level.read_bytes())
    ids = ["multi-a", "multi-b"]
    positions = [[1, 2, 3], [4, 5, 6], [9, 8, 7]]
    for identity, position in zip([*ids, "multi-c"], positions):
        document["nodes"].append({
            "id": identity, "name": identity,
            "components": {
                "azure.transform": component("azure.transform", {
                    "translation": position, "rotation": [0, 0, 0], "scale": [1, 1, 1]}),
                "azure.third-person-camera": component("azure.third-person-camera", {"target": identity}),
            },
        })
    level.write_text(json.dumps(document), encoding="utf-8")
    original_saved = level.read_bytes()

    def choose(host):
        host.click("outliner.search")
        host.call("input", event={"action": "text", "text": "multi-"})
        host.frames()
        host.click("node.multi-a")
        host.click("node.multi-b", ctrl=True)
        assert host.workspace()["selection"] == ids

    def saved_positions():
        data = json.loads(level.read_bytes())
        return {node["id"]: node["components"]["azure.transform"]["data"]["translation"]
                for node in data["nodes"] if node["id"].startswith("multi-")}

    with EditorHost(executable, project_file, output / "edit") as host:
        host.click("save")
        original_unselected = {node["id"]: node for node in json.loads(level.read_bytes())["nodes"]
                               if node["id"] not in ids}
        choose(host)
        original_hash = host.query("document.contentHash")
        initial_undo = host.workspace()["undoCount"]
        check_drag_cancel(host, workflow)
        host.click("save")
        assert saved_positions() == dict(zip([*ids, "multi-c"], positions))
        for target in ("name", "component.add"):
            host.click(target)
        assert host.query("document.contentHash") == original_hash
        assert host.workspace()["selection"] == ids
        host.screenshot("mixed-values-" + workflow)

        host.click("axis.reset.azure.transform.translation.0")
        changed_hash = host.query("document.contentHash")
        assert changed_hash != original_hash
        assert host.workspace()["selection"] == ids
        assert host.workspace()["undoCount"] == initial_undo + 1
        host.click("save")
        expected = {"multi-a": [0, 2, 3], "multi-b": [0, 5, 6], "multi-c": positions[2]}
        assert saved_positions() == expected, saved_positions()
        host.click("menu.Edit")
        host.click("menu.Undo")
        assert host.query("document.contentHash") == original_hash
        assert host.workspace()["selection"] == ids
        host.click("menu.Edit")
        host.click("menu.Redo")
        assert host.query("document.contentHash") == changed_hash
        host.click("save")

        host.click("component.azure.third-person-camera")
        before_cancel = host.query("document.contentHash")
        before_undo = host.workspace()["undoCount"]
        host.click("reference.pick.azure.third-person-camera.target")
        assert host.workspace()["referencePicker"]
        key(host, "Escape")
        assert not host.workspace()["referencePicker"]
        assert host.query("document.contentHash") == before_cancel
        assert host.workspace()["undoCount"] == before_undo
        assert host.workspace()["selection"] == ids
        host.click("reference.pick.azure.third-person-camera.target")
        host.call("input", event={"action": "focus", "focused": False})
        host.frames()
        assert not host.workspace()["referencePicker"]
        host.call("input", event={"action": "focus", "focused": True})
        host.frames()
        assert host.query("document.contentHash") == before_cancel
        host.click("reference.pick.azure.third-person-camera.target")
        host.click("node.multi-c")
        assert not host.workspace()["referencePicker"]
        assert host.workspace()["selection"] == ids
        assert host.workspace()["undoCount"] == before_undo + 1
        host.click("save")
        references = {node["id"]: node["components"]["azure.third-person-camera"]["data"]["target"]
                      for node in json.loads(level.read_bytes())["nodes"]
                      if node["id"].startswith("multi-")}
        assert references == dict.fromkeys([*ids, "multi-c"], "multi-c")
        host.click("menu.Edit")
        host.click("menu.Undo")
        assert host.query("document.contentHash") == before_cancel
        assert host.workspace()["selection"] == ids
        host.click("menu.Edit")
        host.click("menu.Redo")
        host.click("save")
        final_hash = host.query("document.contentHash")
        host.screenshot("mixed-edited-" + workflow)
    assert level.read_bytes() != original_saved
    for node in json.loads(level.read_bytes())["nodes"]:
        if node["id"] not in ids:
            assert node == original_unselected[node["id"]], node["id"]
    with EditorHost(executable, project_file, output / "reopen") as host:
        assert not host.workspace()["dirty"]
        assert host.query("document.contentHash") == final_hash
        assert saved_positions() == expected
        choose(host)
        assert host.workspace()["selection"] == ids
    return {"workflow": workflow, "passed": True, "targets": ids, "untouched": "multi-c",
            "axisPreservesMixedYZ": True, "singleObjectControlsDisabled": True,
            "referenceEscapeAndFocusCancel": True, "referenceCapturedOwners": True,
            "undoRedo": True, "saveReopen": True, "physicalInput": False}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve() / ("run-" + uuid.uuid4().hex[:8])
    output.mkdir(parents=True)
    with args.executable.open("rb") as stream:
        fingerprint = hashlib.file_digest(stream, "sha256").hexdigest()
    results = []
    for workflow in ("exploration", "inspector"):
        results.append(run_case(args.executable.resolve(), output / workflow, workflow))
        (output / "summary.json").write_text(json.dumps({
            "executableSha256": fingerprint, "cases": results}, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"cases": results, "output": str(output)}))
