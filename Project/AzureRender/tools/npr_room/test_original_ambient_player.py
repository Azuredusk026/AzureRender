"""Replay independent original ambient sources through failure, recovery and restart."""
import argparse
import json
import os
from pathlib import Path
from test_notes_player import playing
from test_original_music_player import fixture as music_fixture, digest, write
from test_unlock_room_player import save


def fixture(root, source):
    project = music_fixture(root, source.with_name("music.lua"))
    descriptor = json.loads(project.read_bytes())
    descriptor["runtime"]["input"] = {"ambient-" + name: [] for name in
        ("toggle", "quiet", "loud", "mute", "select-1", "select-2", "page-prev", "page-next")}
    write(project, descriptor)
    assets = root / "assets"
    config = 'local sounds = {{1,"first","assets:/tone-1.wav","Original one"},{2,"second","assets:/tone-2.wav","Original two"}}\n'
    (assets / "ambient.lua").write_text(config + source.read_text(encoding="utf-8"), encoding="utf-8")
    template = source.with_suffix(".rml").read_text(encoding="utf-8")
    buttons = ''.join(f'<button id="ambient-select-{i}" data-action="ambient-select-{i}">Original {i}</button>'
                      for i in (1, 2))
    template = template.replace("{{sounds}}", buttons).replace("</style>",
        '#ambient-toggle { top: 16dp; bottom: auto; left: 16dp; right: auto; } '
        '#ambient-panel { top: 52dp; bottom: auto; left: 16dp; right: auto; }'
        '</style></head><body>')
    (assets / "ambient.rml").write_text('<rml><head><style>body { font-family: LatoLatin; }' + template + '</body></rml>', encoding="utf-8")
    level = json.loads((assets / "room.azurelevel").read_bytes())
    controller = level["nodes"][0]["components"]
    controller.pop("azure.audio-source")
    controller["azure.script"]["data"]["asset"] = "assets:/ambient.lua"
    controller["azure.game-ui"]["data"]["asset"] = "assets:/ambient.rml"
    for i, name in enumerate(("first", "second"), 1):
        level["nodes"].append({"id": name, "components": {"azure.audio-source": {
            "type": "azure.audio-source", "version": 1, "data": {
                "asset": f"assets:/tone-{i}.wav", "volume": 0, "loop": True, "autoplay": False}}}})
    write(assets / "room.azurelevel", level)
    return project


def verify(player, resources, source, output):
    os.environ["VK_INSTANCE_LAYERS"] = "VK_LAYER_KHRONOS_validation"
    os.environ["VK_LOADER_DEBUG"] = "layer"
    output.mkdir()
    project = fixture(output / "fixture", source)
    tone = project.parent / "assets/tone-1.wav"
    original = tone.read_bytes()
    tone.write_bytes(original[:16])
    profile, control = output / "profile", {}
    save(profile, project, "ambient", [1, [[1, 45, False], [2, 30, False]]])
    slot, = profile.glob("project-*/save-ambient.json")
    try:
        with playing(player, project, resources, profile, output / "recover",
                     on_ready=lambda _, call: control.update(call=call),
                     expected_presentation_errors=("Truncated WAV container",)) as (click, _, _, screenshot):
            click("ambient-toggle")
            state = json.loads(control["call"]("query", name="game.state"))
            assert state["audioStarts"] == 1 and "Truncated WAV container" in state["presentationErrors"], state
            screenshot("unavailable.png")
            tone.write_bytes(original)
            click("ambient-loud")
            assert json.loads(control["call"]("query", name="game.state"))["audioStarts"] == 2
            expected = [1, [[1, 50, False], [2, 30, False]]]
            assert json.loads(slot.read_bytes())["payload"] == expected
            screenshot("recovered.png")
            stored = slot.read_bytes()
            protected = b'{"schemaVersion":2,"payload":["preserve"]}'
            slot.write_bytes(protected)
            click("ambient-quiet")
            assert slot.read_bytes() == protected
            screenshot("write-failed.png")
            slot.write_bytes(stored)
            click("ambient-loud")
            assert json.loads(slot.read_bytes())["payload"] == expected
    finally:
        tone.write_bytes(original)
    with playing(player, project, resources, profile, output / "reopen", 2) as (click, _, _, screenshot):
        click("ambient-toggle")
        click("ambient-mute")
        assert json.loads(slot.read_bytes())["payload"] == [1, [[1, 50, True], [2, 30, False]]]
        screenshot("reopened.png")
    for folder in ("recover", "reopen"):
        text = (output / folder / "stderr.log").read_text(encoding="utf-8")
        assert 'Insert instance layer "VK_LAYER_KHRONOS_validation"' in text
        assert "VUID-" not in text and "SYNC-HAZARD" not in text
    result = {"playerSHA256": digest(player), "controllerSHA256": digest(source),
              "originalToneSHA256": digest(tone), "independentPlayers": 2,
              "expectedDiagnostic": "Truncated WAV container", "scriptErrors": [],
              "liveAllocationsAfterExit": 0, "unaffectedSecondSource": True,
              "protectedFutureSaveAndRetried": True, "preferences": json.loads(slot.read_bytes())["payload"]}
    write(output / "verification.json", result)
    print(json.dumps(result))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("player", "resources", "source", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    verify(args.player, args.resources, args.source, args.output)
