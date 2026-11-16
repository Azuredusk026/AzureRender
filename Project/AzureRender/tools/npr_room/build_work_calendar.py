"""Stage the work calendar beside the room's timer without changing the base project."""

import argparse
import json
import os
from pathlib import Path
import shutil
import tempfile

from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.pbr_materials import write
from tools.npr_room.room_geometry import ui_font_document


def stage_calendar(room, output):
    examples = Path(__file__).resolve().parents[2] / "examples/npr-room/assets"
    descriptor = json.loads((room / "project.azureproject").read_bytes())
    inputs = descriptor["runtime"]["input"]
    if "timer-stats" in inputs:
        raise ValueError("Room already has the work calendar")
    inputs["timer-stats"] = []

    def copy_asset(path, destination):
        return os.link(path, destination) if Path(path).suffix.lower() == ".glb" else shutil.copyfile(path, destination)

    with tempfile.TemporaryDirectory(prefix="work-calendar-", dir=output.parent) as holder:
        project = Path(holder) / "project"
        assets = project / "assets"
        shutil.copytree(room / "assets", assets, copy_function=copy_asset, ignore=shutil.ignore_patterns(".azure"))
        write(project / "project.azureproject", descriptor)
        controller = (examples / "work-calendar.lua").read_bytes() + b"\n" + (examples / "pomodoro.lua").read_bytes()
        (assets / "pomodoro.lua").write_bytes(controller)
        rml = (examples / "timer.rml").read_text(encoding="utf-8")
        fonts = sorted(path.name for path in assets.glob("ui-fallback.*") if path.suffix.lower() in (".otf", ".ttf"))
        if fonts:
            font, = fonts
            rml = ui_font_document(rml, font)
        (assets / "timer.rml").write_text(rml, encoding="utf-8", newline="\n")
        publish_directory(project, output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--room", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    stage_calendar(args.room, args.output)


if __name__ == "__main__":
    main()
