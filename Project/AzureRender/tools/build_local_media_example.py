"""Create an original media workspace using the public Player components."""
import argparse
import json
from pathlib import Path
import shutil
import struct
import wave


ACTIONS = ["media-" + name for name in (
    "import-music", "folder-music", "import-images", "folder-images", "cancel", "refresh",
    "music-prev", "music-next", "play", "image-prev", "image-next")]


def build(output):
    output.mkdir()
    assets = output / "assets"
    assets.mkdir()
    source = Path(__file__).resolve().parents[1] / "examples/local-media/assets"
    for name in ("media.lua", "media.rml"):
        shutil.copyfile(source / name, assets / name)
    with wave.open(str(assets / "placeholder.wav"), "wb") as audio:
        audio.setparams((1, 2, 8000, 0, "NONE", "not compressed"))
        audio.writeframes(struct.pack("<h", 0) * 800)
    def write(path, value):
        path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    write(output / "project.azureproject", {
        "schemaVersion": 1, "id": "a389a3ec-703d-4db0-b3b5-a4c4e3649cb1", "name": "本地媒体工作间",
        "startupScene": "assets:/room.azurelevel", "mounts": [{"name": "assets", "path": "assets"}],
        "runtime": {"schemaVersion": 1, "input": {name: [] for name in ACTIONS}, "systems": []}})
    def component(kind, data):
        return {"type": kind, "version": 1, "data": data}
    write(assets / "room.azurelevel", {"schemaVersion": 1, "id": "media-workspace", "sceneType": "sample",
        "resources": [], "lights": [], "nodes": [{"id": "media-controller", "components": {
            "azure.script": component("azure.script", {"asset": "assets:/media.lua"}),
            "azure.game-ui": component("azure.game-ui", {"asset": "assets:/media.rml"}),
            "azure.audio-source": component("azure.audio-source", {
                "asset": "assets:/placeholder.wav", "autoplay": False, "volume": .4, "loop": False})}}]})
    return output / "project.azureproject"


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    print(build(parser.parse_args().output))
