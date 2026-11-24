"""Add the task-list controller to a prepared room project."""

import argparse
import json
import os
from pathlib import Path
import shutil
import tempfile

from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.pbr_materials import write
from tools.npr_room.room_geometry import TODO_ACTIONS, activate_room_control, install_todo_audio, ui_font_document


def stage_todo(room, output, source, playback):
    example = Path(__file__).resolve().parents[2] / "examples/npr-room/assets"
    descriptor = json.loads((room / "project.azureproject").read_bytes())

    def copy_asset(path, destination):
        return os.link(path, destination) if Path(path).suffix.lower() == ".glb" else shutil.copyfile(path, destination)

    with tempfile.TemporaryDirectory(prefix="room-todo-", dir=output.parent) as parent:
        project = Path(parent) / "project"
        assets = project / "assets"
        shutil.copytree(room / "assets", assets, copy_function=copy_asset, ignore=shutil.ignore_patterns(".azure"))
        level_path = assets / "room.azurelevel"
        level = json.loads(level_path.read_bytes())
        activate_room_control(descriptor, level, "todo", TODO_ACTIONS)
        install_todo_audio(source, playback, assets, level)
        write(project / "project.azureproject", descriptor)
        write(level_path, level)
        shutil.copyfile(example / "todo.lua", assets / "todo.lua")
        rml = (example / "timer.rml").read_text(encoding="utf-8")
        fonts = sorted(path.name for path in assets.glob("ui-fallback.*") if path.suffix.lower() in (".otf", ".ttf"))
        if fonts:
            font, = fonts
            rml = ui_font_document(rml, font)
        (assets / "timer.rml").write_text(rml, encoding="utf-8", newline="\n")
        publish_directory(project, output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("room", "output", "source", "audio-playback"):
        parser.add_argument("--" + argument, type=Path, required=True)
    args = parser.parse_args()
    stage_todo(args.room, args.output, args.source, args.audio_playback)


if __name__ == "__main__":
    main()
