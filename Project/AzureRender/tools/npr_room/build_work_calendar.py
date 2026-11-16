"""Stage the work calendar beside the room's timer without changing the base project."""

import argparse
import json
import math
import os
from pathlib import Path
import shutil
import tempfile

from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest
from tools.npr_room.room_geometry import ui_font_document
from tools.npr_room.skinned_mesh_export import UnityScene


PARAMETER_ASSET = "Assets/App/BulbulAssets/MasterData/ScriptableObjects/GameParameterMaster.asset"


def work_rewards(source):
    path = source / PARAMETER_ASSET
    master = UnityScene(path).read(11400000)

    def extract(key, stem):
        row = master[key]
        return {name: row[stem + suffix] for name, suffix in
                (("minutes", "Minutes"), ("perBlock", "Max"), ("maxHours", "MaxHour"))}

    level = master["LevelUpInfoData"]
    exp = extract("LevelUpInfoData", "pomodoroExpBase")
    points = extract("PointUpInfoData", "pomodoroPointBase")
    thresholds = level["nextLevelNecessaryExpArray"]
    fallback = level["nextLevelNecessaryExpBase"]
    if len(thresholds) < 31 or any(not math.isfinite(v) or v <= 0 for v in
                                  [*exp.values(), *points.values(), *thresholds, fallback]):
        raise ValueError("Work rewards require positive source rates and level thresholds")
    return {"exp": exp, "points": points, "levelThresholds": thresholds,
            "fallbackThreshold": fallback, "sourceSHA256": digest(path)}


def reward_config(properties):
    def fields(values):
        return "{" + ", ".join(f"{key} = {value}" for key, value in values.items()) + "}"

    thresholds = ", ".join(str(value) for value in properties["levelThresholds"])
    return (
        "work_reward_config = {\n"
        f"    exp = {fields(properties['exp'])},\n"
        f"    points = {fields(properties['points'])},\n"
        f"    levelThresholds = {{{thresholds}}},\n"
        f"    fallbackThreshold = {properties['fallbackThreshold']},\n"
        "}\n"
    ).encode("utf-8")


def stage_calendar(room, output, source=None):
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
        parts = []
        if source is not None:
            rewards = work_rewards(source)
            parts.extend((reward_config(rewards), (examples / "work-progression.lua").read_bytes()))
            write(project / "work-progression-receipt.json", rewards)
        parts.extend((examples / name).read_bytes() for name in ("work-calendar.lua", "pomodoro.lua"))
        (assets / "pomodoro.lua").write_bytes(b"\n".join(parts))
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
    parser.add_argument("--source", type=Path, help="Bind the original work reward parameters")
    args = parser.parse_args()
    stage_calendar(args.room, args.output, args.source)


if __name__ == "__main__":
    main()
