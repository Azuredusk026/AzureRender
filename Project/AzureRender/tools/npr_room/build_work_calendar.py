"""Stage the work calendar beside the room's timer without changing the base project."""

import argparse
import json
import math
import os
import re
from pathlib import Path
import shutil
import tempfile

from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest
from tools.npr_room.room_geometry import copy_room_asset, ui_font_document
from tools.npr_room.skinned_mesh_export import UnityScene


PARAMETER_ASSET = "Assets/App/BulbulAssets/MasterData/ScriptableObjects/GameParameterMaster.asset"
CALENDAR_SCRIPTS = ("habit-tracker.lua", "habit-calendar-menu.lua", "work-calendar.lua")


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


def calendar_script(examples, rewards=None):
    parts = []
    if rewards is not None:
        parts.extend((reward_config(rewards), (examples / "work-progression.lua").read_bytes()))
    parts.extend((examples / name).read_bytes() for name in (*CALENDAR_SCRIPTS, "pomodoro.lua"))
    return b"\n".join(parts)


def calendar_actions():
    return ("timer-stats", "calendar-prev-month", "calendar-next-month", "calendar-edit-time", "calendar-submit-time",
            "calendar-diary-toggle", "calendar-diary-save", "calendar-diary-cancel",
            "calendar-habits-toggle",
            *(f"calendar-habit-{action}" for action in ("new", "save", "prev", "next", "complete", "pause",
                                                       "move-up", "move-down", "archive", "archive-confirm",
                                                       "archive-cancel")),
            *(f"calendar-habit-day-{weekday}" for weekday in range(7)),
            *(f"calendar-first-{weekday}" for weekday in range(7)),
            *(f"calendar-day-{cell}" for cell in range(1, 43)))


def integrate_calendar(project, document, program, examples, localization=""):
    """Refresh the calendar in a room assembled with the work statistics panel."""
    legacy_style = "#work-stats { display: block; margin-top: 8dp; font-size: 13dp; }"
    progression = '<span id="work-progression" style="display: none;"></span>'
    legacy_update = "            local growth = workCalendar:progress()"
    legacy_init = "workCalendar = new_work_calendar()"
    legacy_toggle = '        if self:pressed("timer-stats") then'
    legacy_shutdown = "    if workCalendar then workCalendar:flush() end"
    module_start = 'local workSlot = "work-calendar"'
    module_end = "\n    return calendar\nend\n"
    if ('id="work-calendar"' in document or "workCalendar:update_month_view(" in program
            or any(document.count(anchor) != 1 for anchor in (legacy_style, progression))
            or any(program.count(anchor) != 1 for anchor in
                   (module_start, module_end, legacy_init, legacy_update, legacy_toggle, legacy_shutdown))):
        raise ValueError("Expected one assembled room with the original work statistics panel")
    template = (examples / "timer.rml").read_text(encoding="utf-8")
    styles = template[template.index("#work-stats"):template.index("#notes-toggle")]
    body = template.split("<!-- work-calendar-start -->", 1)[1].split("<!-- work-calendar-end -->", 1)[0]
    document = document.replace(legacy_style, styles.rstrip(), 1).replace(progression, progression + body, 1)
    start = program.index(module_start)
    end = program.index(module_end, start) + len(module_end)
    module = "\n".join((examples / name).read_text(encoding="utf-8") for name in CALENDAR_SCRIPTS)
    program = program[:start] + localization + module + program[end:]
    program = program.replace(legacy_init, "workCalendar = new_work_calendar(translate)", 1)
    program = program.replace(legacy_toggle,
                              '        if self:pressed("timer-stats") and (not statsOpen or workCalendar:close_month_view()) then',
                              1)
    program = program.replace(legacy_shutdown, "    if workCalendar then\n        workCalendar:save_open_diary()\n"
                              "        workCalendar:flush()\n    end", 1)
    sample = (examples / "pomodoro.lua").read_text(encoding="utf-8")
    update = sample[sample.index("            local date, time = workCalendar:update_month_view("):
                    sample.index(legacy_update)]
    program = program.replace(legacy_update, update + legacy_update, 1)
    actions = project["runtime"]["input"]
    for name in calendar_actions():
        actions.setdefault(name, [])
    return program, document


def stage_calendar(room, output, source=None):
    examples = Path(__file__).resolve().parents[2] / "examples/npr-room/assets"
    descriptor = json.loads((room / "project.azureproject").read_bytes())
    inputs = descriptor["runtime"]["input"]
    if "timer-stats" in inputs:
        raise ValueError("Room already has the work calendar")
    inputs.update({name: [] for name in calendar_actions()})

    def copy_asset(path, destination):
        return os.link(path, destination) if Path(path).suffix.lower() == ".glb" else shutil.copyfile(path, destination)

    with tempfile.TemporaryDirectory(prefix="work-calendar-", dir=output.parent) as holder:
        project = Path(holder) / "project"
        assets = project / "assets"
        shutil.copytree(room / "assets", assets, copy_function=copy_asset, ignore=shutil.ignore_patterns(".azure"))
        write(project / "project.azureproject", descriptor)
        rewards = work_rewards(source) if source is not None else None
        if rewards is not None:
            write(project / "work-progression-receipt.json", rewards)
        (assets / "pomodoro.lua").write_bytes(calendar_script(examples, rewards))
        rml = (examples / "timer.rml").read_text(encoding="utf-8")
        fonts = sorted(path.name for path in assets.glob("ui-fallback.*") if path.suffix.lower() in (".otf", ".ttf"))
        if fonts:
            font, = fonts
            rml = ui_font_document(rml, font)
        (assets / "timer.rml").write_text(rml, encoding="utf-8", newline="\n")
        publish_directory(project, output)


def refresh_integrated_calendar(room, output, inventory):
    room, output = room.resolve(strict=True), output.resolve()
    if output.exists():
        raise FileExistsError(output)
    if output.is_relative_to(room):
        raise ValueError("The calendar output must be outside its input room")
    from tools.npr_room.room_language import calendar_config, calendar_strings

    language = json.loads((room / "language-receipt.json").read_bytes())
    if language["storySHA256"] != digest(inventory):
        raise ValueError("The calendar must use the original room language inventory")
    examples = Path(__file__).resolve().parents[2] / "examples/npr-room/assets"
    labels = calendar_strings(json.loads(inventory.read_bytes())["localization"])
    project_path = room / "project.azureproject"
    descriptor = json.loads(project_path.read_bytes())
    mount, = (row for row in descriptor["mounts"] if row["name"] == "assets")
    if mount["path"] != "assets":
        raise ValueError("The integrated calendar requires the room assets mount")
    inputs = room / "assets"
    program, document = integrate_calendar(
        descriptor, (inputs / "timer.rml").read_text(encoding="utf-8"),
        (inputs / "pomodoro.lua").read_text(encoding="utf-8"), examples, calendar_config(labels))
    prefix, anchor, previous = program.rpartition('local slot = "pomodoro-settings"\n')
    if not anchor or not previous.rstrip().endswith("\nend"):
        raise ValueError("The room must end with its timer controller")
    current = (examples / "pomodoro.lua").read_text(encoding="utf-8")
    if "Points earned %d" in previous:
        current = current.replace("Points %d", "Points earned %d")
    program = prefix + current
    for element in ("room-language-toggle", "room-language-panel"):
        selector = re.compile(r"(#" + element + r"\s*\{[^}]*?left:)\s*42%;")
        document, count = selector.subn(r"\g<1> 140dp;", document)
        if count != 1:
            raise ValueError(f"Expected the integrated room language position: {element}")
    receipt = {"schemaVersion": 1, "inputProjectSHA256": digest(project_path),
               "inputControllerSHA256": digest(inputs / "pomodoro.lua"), "inputDocumentSHA256": digest(inputs / "timer.rml"),
               "sourceInventorySHA256": digest(inventory), "timerSourceSHA256": digest(examples / "pomodoro.lua"),
               "startupSourceSHA256": digest(examples / "startup-flow.lua"),
               "modules": {name: digest(examples / name) for name in CALENDAR_SCRIPTS}}
    with tempfile.TemporaryDirectory(prefix="integrated-calendar-", dir=output.parent) as folder:
        staged = Path(folder) / "room"
        assets = staged / "assets"
        shutil.copytree(inputs, assets, copy_function=copy_room_asset, ignore=shutil.ignore_patterns(".azure"))
        for prior in room.glob("*-receipt.json"):
            shutil.copyfile(prior, staged / prior.name)
        shutil.copyfile(examples / "startup-flow.lua", assets / "startup-flow.lua")
        (assets / "pomodoro.lua").write_text(program, encoding="utf-8", newline="\n")
        (assets / "timer.rml").write_text(document, encoding="utf-8", newline="\n")
        receipt.update(controllerSHA256=digest(assets / "pomodoro.lua"), documentSHA256=digest(assets / "timer.rml"))
        write(staged / project_path.name, descriptor)
        write(staged / "calendar-room-receipt.json", receipt)
        publish_directory(staged, output)
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--room", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--source", type=Path, help="Bind the original work reward parameters")
    parser.add_argument("--refresh-integrated", type=Path, metavar="STORY_INVENTORY")
    args = parser.parse_args()
    if args.refresh_integrated is not None:
        if args.source is not None:
            parser.error("--refresh-integrated uses the existing room reward receipt")
        refresh_integrated_calendar(args.room, args.output, args.refresh_integrated)
    else:
        stage_calendar(args.room, args.output, args.source)


if __name__ == "__main__":
    main()
