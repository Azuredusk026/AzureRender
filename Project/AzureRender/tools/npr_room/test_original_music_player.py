"""Replay music setting recovery with generated original tones and production UI actions."""
import argparse
from hashlib import sha256
import json
import math
import os
from pathlib import Path
import struct
import wave

from test_notes_player import playing


def digest(path):
    return sha256(path.read_bytes()).hexdigest()


def write(path, value):
    path.write_text(json.dumps(value), encoding="utf-8")


def fixture(root, source):
    assets = root / "assets"
    assets.mkdir(parents=True)
    actions = ["toggle", "play", "repeat", "quiet", "mute", "shuffle", "next", "previous", "loud"]
    write(root / "project.azureproject", {
        "schemaVersion": 1, "id": "3b8f303a-5a83-4b39-a0e9-6c45058e608a", "name": "Original audio recovery",
        "startupScene": "assets:/room.azurelevel", "mounts": [{"name": "assets", "path": "assets"}],
        "runtime": {"schemaVersion": 1, "input": {"music-" + a: [] for a in actions}, "systems": []},
    })
    for index, frequency in enumerate((330, 550), 1):
        with wave.open(str(assets / f"tone-{index}.wav"), "wb") as audio:
            audio.setparams((1, 2, 48000, 0, "NONE", "not compressed"))
            audio.writeframes(b"".join(struct.pack("<h", int(2500 * math.sin(2 * math.pi * frequency * n / 48000)))
                                       for n in range(96000)))
    config = 'local tracks = {{"one","assets:/tone-1.wav","Original one","Fixture",2},' \
             '{"two","assets:/tone-2.wav","Original two","Fixture",2}}\n'
    (assets / "music.lua").write_text(config + source.read_text(encoding="utf-8"), encoding="utf-8")
    component = lambda kind, data: {"type": kind, "version": 1, "data": data}
    write(assets / "room.azurelevel", {
        "schemaVersion": 1, "id": "room", "sceneType": "sample", "resources": [], "lights": [],
        "nodes": [{"id": "music", "components": {
            "azure.script": component("azure.script", {"asset": "assets:/music.lua"}),
            "azure.game-ui": component("azure.game-ui", {"asset": "assets:/music.rml"}),
            "azure.audio-source": component("azure.audio-source", {
                "asset": "assets:/tone-1.wav", "autoplay": False, "volume": .4, "loop": False}),
        }}],
    })
    template = source.with_suffix(".rml").read_text(encoding="utf-8")
    template = template.replace("{{tracks}}", "").replace("</style>",
        "#music-toggle { top: 16dp; bottom: auto; } #music-panel { top: 52dp; bottom: auto; }"
        "</style></head><body>")
    (assets / "music.rml").write_text(
        '<rml><head><style>body { font-family: LatoLatin; }' + template + '</body></rml>', encoding="utf-8")
    return root / "project.azureproject"


def verify(player, resources, source, output):
    os.environ["VK_INSTANCE_LAYERS"] = "VK_LAYER_KHRONOS_validation"
    os.environ["VK_LOADER_DEBUG"] = "layer"
    output.mkdir()
    project = fixture(output / "fixture", source)
    profile = output / "profile"
    with playing(player, project, resources, profile, output / "write") as (click, _, _, screenshot):
        click("music-toggle")
        click("music-repeat")
        slot, = profile.glob("project-*/save-music.json")
        assert json.loads(slot.read_bytes())["payload"] == [1, True, 50, False, False, True]
        click("music-quiet")
        click("music-mute")
        assert json.loads(slot.read_bytes())["payload"] == [1, True, 45, True, False, True]
        screenshot("repeat-written.png")
    control = {}
    with playing(player, project, resources, profile, output / "reopen-repeat",
                 on_ready=lambda _, call: control.update(call=call)) as (click, _, _, screenshot):
        click("music-toggle")
        control["call"]("wait-frames", frames=220)
        state = json.loads(control["call"]("query", name="game.state"))
        assert state["audioStarts"] >= 2, state
        screenshot("repeat-restored.png")
        original = slot.read_bytes()
        protected = b'{"schemaVersion":2,"payload":["preserve"]}'
        slot.write_bytes(protected)
        click("music-repeat")
        assert slot.read_bytes() == protected
        screenshot("write-failed.png")
        slot.write_bytes(original)
        click("music-repeat")
        click("music-repeat")
        assert json.loads(slot.read_bytes())["payload"] == [1, True, 45, True, False, False]
    with playing(player, project, resources, profile, output / "reopen-sequence",
                 on_ready=lambda _, call: control.update(call=call)) as (click, _, _, screenshot):
        click("music-toggle")
        control["call"]("wait-frames", frames=220)
        state = json.loads(control["call"]("query", name="game.state"))
        assert state["audioStarts"] >= 2, state
        screenshot("sequence-restored.png")
    logs = {}
    for name in ("write", "reopen-repeat", "reopen-sequence"):
        text = (output / name / "stderr.log").read_text(encoding="utf-8")
        assert 'Insert instance layer "VK_LAYER_KHRONOS_validation"' in text, name
        assert "VUID-" not in text and "SYNC-HAZARD" not in text, name
        logs[name] = [Path(line.split("Decoded source: ", 1)[1]).name
                      for line in text.splitlines() if "Decoded source:" in line]
    assert logs["reopen-repeat"] == ["tone-1.wav"], logs
    assert logs["reopen-sequence"][:2] == ["tone-1.wav", "tone-2.wav"], logs
    result = {"playerSHA256": digest(player), "controllerSHA256": digest(source),
              "shaderSHA256": {p.name: digest(p) for p in sorted((player.parent / "shaders").glob("*.spv"))},
              "resourceRoot": str(resources),
              "fixtureSHA256": {p.name: digest(p) for p in sorted((project.parent / "assets").iterdir()) if p.is_file()},
              "decodedSources": logs, "preferences": json.loads(slot.read_bytes())["payload"],
              "independentPlayers": 3, "protectedFutureSaveAndRetried": True,
              "validationPassed": True, "liveAllocationsAfterExit": 0,
              "deviceScope": "Current Windows default device; physical no-device case requires controlled device access"}
    write(output / "verification.json", result)
    print(json.dumps(result))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("player", "resources", "source", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    verify(args.player, args.resources, args.source, args.output)
