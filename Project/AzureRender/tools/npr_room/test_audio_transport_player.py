"""Replay pause, gain, resume and replacement of real Recovery streaming music."""

import argparse
import json
import os
from pathlib import Path
import subprocess

from reference_inventory import digest


TRANSPORT = """
local state = "start"
local hold, pausedAt, resumeAt
function init()
    self:audio_play()
end
function update()
    if state == "start" and self:audio_position() > .05 then
        assert(self:audio_playing())
        self:audio_pause(true)
        state = "paused"
        hold = 0
    elseif state == "paused" then
        hold = hold + 1
        assert(not self:audio_playing())
        if hold == 2 then
            pausedAt = self:audio_position()
            self:set("azure.audio-source", "volume", .25)
        elseif hold == 8 then
            assert(self:audio_position() == pausedAt and pausedAt > .05)
            self:audio_pause(false)
            state = "resumed"
        end
    elseif state == "resumed" and self:audio_position() > pausedAt + .02 then
        resumeAt = self:audio_position()
        self:audio_stop()
        assert(not self:audio_playing())
        self:set("azure.audio-source", "asset", "assets:/childream.ogg")
        assert(self:audio_position() == 0)
        self:audio_play()
        state = "changed"
    elseif state == "changed" and self:audio_position() > .025 then
        assert(self:audio_playing())
        self:set("azure.transform", "translation", {2, pausedAt, resumeAt})
        self:audio_stop()
        state = "finished"
    end
end
"""


def write(path, document):
    path.write_text(json.dumps(document), encoding="utf-8")


def replay(player, unity_source, resources, output):
    output.mkdir()
    assets = output / "assets"
    assets.mkdir()
    music = unity_source / "Assets/App/BulbulAssets/Audio/Music"
    for source, name in (("aqua room.ogg", "aqua.ogg"), ("ChilDream.ogg", "childream.ogg")):
        os.link(music / source, assets / name)

    project = output / "project.azureproject"
    write(project, {"schemaVersion": 1, "id": "f8cb8f11-fa59-4edb-89d6-c85b533be903",
                    "name": "Room Audio Transport", "startupScene": "assets:/audio.azurelevel",
                    "mounts": [{"name": "assets", "path": "assets"}]})
    write(assets / "audio.azurelevel", {"schemaVersion": 1, "id": "audio", "sceneType": "sample",
          "resources": [], "nodes": [{"id": "music", "components": {
              "azure.audio-source": {"type": "azure.audio-source", "version": 1,
                                     "data": {"asset": "assets:/aqua.ogg", "volume": .5, "loop": True}},
              "azure.script": {"type": "azure.script", "version": 1,
                               "data": {"asset": "assets:/transport.lua"}},
          }}]})
    (assets / "transport.lua").write_text(TRANSPORT, encoding="utf-8")

    report = output / "runtime.json"
    result = subprocess.run([
        str(player), "--project", str(project), "--resource-root", str(resources), "--width", "640",
        "--height", "360", "--fixed-frame-step", "--smoke-frames", "420", "--runtime-report", str(report),
    ], cwd=output, capture_output=True, encoding="utf-8", errors="replace", timeout=180)
    (output / "stdout.log").write_text(result.stdout, encoding="utf-8")
    (output / "stderr.log").write_text(result.stderr, encoding="utf-8")
    assert result.returncode == 0, result.stderr[-3500:]
    assert "Allocator after unload: buffers=0 images=0" in result.stdout
    assert result.stderr.count("Streaming source:") == 2, result.stderr[-3500:]
    stats = json.loads(report.read_bytes())
    assert not stats["scriptErrors"] and not stats["presentationErrors"] and stats["audioStarts"] == 2, stats
    node, = (row for row in stats["nodes"] if row["id"] == "music")
    passed, paused, resumed = node["translation"]
    assert passed == 2 and resumed > paused > .05, node
    proof = {"audioStarts": stats["audioStarts"], "pausedAt": paused, "resumedAt": resumed,
             "sourceSHA256": {path.name: digest(path) for path in sorted(assets.glob("*.ogg"))}}
    write(output / "transport-proof.json", proof)
    return proof


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("player", "source", "resource-root", "output"):
        parser.add_argument("--" + argument, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(replay(args.player.resolve(), args.source.resolve(), args.resource_root.resolve(),
                            args.output.resolve())))
