"""Replay node and asset reference searches in two editor projects."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import wave

from create_playable_project import create
from editor_test_host import EditorHost
from test_editor_multi_properties import component, key

OWNER = "search-reference-owner"
TARGET = "search-reference-target"
NODE = "azure.third-person-camera.target"
ASSET = "azure.audio-source.asset"
SOUND = "assets:/search-reference-sound.wav"
NO_MATCH = "absent-ref-xyz"


def prepare(workflow, project):
    if workflow == "exploration":
        create(project)
    else:
        source = Path(__file__).resolve().parents[1] / "assets_public/scene_inspector"
        shutil.copytree(source, project, ignore=shutil.ignore_patterns(".azure"))
    project_file = project / "project.azureproject"
    assets = project / "assets"
    descriptor = json.loads(project_file.read_bytes())
    level = assets / descriptor["startupScene"].removeprefix("assets:/")
    scene = json.loads(level.read_bytes())
    scene["nodes"].extend([
        {"id": OWNER, "components": {
            "azure.third-person-camera": component("azure.third-person-camera", {"target": OWNER}),
            "azure.audio-source": component("azure.audio-source", {}),
        }},
        {"id": TARGET, "name": "Reference destination"},
    ])
    level.write_text(json.dumps(scene), encoding="utf-8")
    with wave.open(str(assets / SOUND.removeprefix("assets:/")), "wb") as wav:
        wav.setparams((1, 2, 8000, 0, "NONE", "not compressed"))
        wav.writeframes(bytes(160))
    return project_file, level


def type_query(host, value):
    host.call("input", event={"action": "text", "text": value})
    host.frames()
    return host.workspace()["widgets"]


def historical_replay(host, workflow):
    # Baseline exposes the Pick button, immediately below the combo, as a replay anchor.
    def open_legacy(field):
        pick = "reference.pick." + field
        _, _, _, height = host.workspace()["widgets"][pick]
        host.click(pick, offset=[0, -height - 4])
        return pick

    pick = open_legacy(NODE)
    type_query(host, NO_MATCH)
    host.screenshot("reference-baseline-direct-" + workflow)
    host.click(pick, offset=[0, 4])
    type_query(host, NO_MATCH)
    host.screenshot("reference-baseline-empty-" + workflow)
    host.click("outliner.search")
    open_legacy(ASSET)
    host.screenshot("reference-baseline-switched-" + workflow)


def verify(host, workflow, level):
    def option(field, item):
        return "reference.option." + field + "." + item

    def check_edit(field, query, item):
        before = host.query("document.contentHash")
        history = host.workspace()["undoCount"]
        host.click("reference.combo." + field)
        widgets = type_query(host, query)
        assert option(field, item) in widgets
        host.click(option(field, item))
        after = host.query("document.contentHash")
        assert after != before
        assert host.workspace()["undoCount"] == history + 1
        host.click("menu.Edit")
        host.click("menu.Undo")
        assert host.query("document.contentHash") == before
        host.click("menu.Edit")
        host.click("menu.Redo")
        assert host.query("document.contentHash") == after
        assert host.workspace()["selection"] == [OWNER]

    before = host.query("document.contentHash")
    state = host.workspace()
    host.click("reference.combo." + NODE)
    assert option(NODE, TARGET) in host.workspace()["widgets"]
    widgets = type_query(host, NO_MATCH)
    assert "reference.no-matches." + NODE in widgets
    assert option(NODE, TARGET) not in widgets
    host.screenshot("reference-empty-" + workflow)
    host.click("reference.clear-search." + NODE)
    assert option(NODE, TARGET) in host.workspace()["widgets"]
    widgets = type_query(host, "Reference destination")
    assert option(NODE, OWNER) not in widgets and option(NODE, TARGET) in widgets
    host.click("outliner.search")
    host.click("reference.combo." + ASSET)
    widgets = host.workspace()["widgets"]
    assert {name for name in widgets if name.startswith("reference.option." + ASSET + ".")} == {option(ASSET, SOUND)}
    assert "reference.clear-search." + ASSET not in widgets
    host.screenshot("reference-switched-" + workflow)
    widgets = type_query(host, "absent-audio")
    assert "reference.no-matches." + ASSET in widgets
    key(host, "Escape")
    assert "reference.search." + ASSET not in host.workspace()["widgets"]
    host.click("reference.combo." + NODE)
    widgets = host.workspace()["widgets"]
    assert option(NODE, OWNER) in widgets and option(NODE, TARGET) in widgets
    assert "reference.clear-search." + NODE not in widgets
    key(host, "Escape")
    assert host.query("document.contentHash") == before
    after = host.workspace()
    for field in ("selection", "undoCount", "dirty"):
        assert after[field] == state[field]

    check_edit(NODE, TARGET, TARGET)
    check_edit(ASSET, "search-reference-sound", SOUND)
    host.click("save")
    assert not host.workspace()["dirty"]
    nodes = json.loads(level.read_bytes())["nodes"]
    fields = next(node["components"] for node in nodes if node["id"] == OWNER)
    assert fields["azure.third-person-camera"]["data"]["target"] == TARGET
    sidecar = level.parent / (SOUND.removeprefix("assets:/") + ".azmeta")
    asset_id = json.loads(sidecar.read_bytes())["id"]
    assert fields["azure.audio-source"]["data"]["asset"] in (asset_id, SOUND)
    return host.query("document.contentHash")


def run(executable, output, baseline):
    output.mkdir(parents=True)
    cases = []
    for workflow in ("exploration", "inspector"):
        directory = output / workflow
        project, level = prepare(workflow, directory / "project")
        with EditorHost(executable, project, directory / "edit") as host:
            host.click("save")
            host.click("outliner.search")
            type_query(host, OWNER)
            host.click("node." + OWNER)
            host.click("component.azure.third-person-camera")
            host.click("component.azure.audio-source")
            if baseline:
                historical_replay(host, workflow)
            else:
                fingerprint = verify(host, workflow, level)
        if not baseline:
            with EditorHost(executable, project, directory / "reopen") as host:
                assert host.query("document.contentHash") == fingerprint
                assert not host.workspace()["dirty"]
        cases.append({"workflow": workflow, "passed": not baseline, "physicalInput": False})
    result = {"executableSha256": hashlib.sha256(executable.read_bytes()).hexdigest(), "cases": cases}
    (output / "summary.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--baseline", action="store_true")
    args = parser.parse_args()
    run(args.executable.resolve(), args.output.resolve(), args.baseline)
