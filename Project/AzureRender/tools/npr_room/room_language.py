"""Load the original Windows language IDs and build the localized room."""

import argparse
from html import escape
import json
import os
from pathlib import Path
import re
import shutil
import tempfile
from uuid import UUID

from tools.npr_room.build_choice_room import choice_config
from tools.npr_room.build_voice_room import localized_text_config, voice_config
from tools.npr_room.build_work_calendar import calendar_script
from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest
from tools.npr_room.room_geometry import copy_room_asset


GAME_LANGUAGE = "Assets/Scripts/Assembly-CSharp/Bulbul/GameLanguageType.cs"
LOCALES = (
    ("Japanese", "Ja", "日本語"),
    ("English", "En", "English"),
    ("ChineseSimplified", "ZhHans", "简体中文"),
    ("ChineseTraditional", "ZhHant", "繁體中文"),
    ("Portuguese", "Pt", "Português"),
    ("Korean", "Ko", "한국어"),
    ("Russian", "Ru", "Русский"),
)
FIELDS = tuple(field for _, field, _ in LOCALES)
CALENDAR_WEEKDAYS = tuple("ui_calendar_" + day for day in
                          ("sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday"))
CALENDAR_LABELS = {"firstDay": "ui_setting_general_firstday_title", "workTime": "ui_calendar_group_work_title",
                   "editTime": "ui_calendar_worktime_edit_title", "error": "ui_common_error_body",
                   "completedTasks": "ui_calendar_group_todo_title"}
SOURCE_FONTS = (
    ("ed1ce6b1b31e42f4a9a27b428b87f28c", "IBMPlexSansJP-Medium.otf", "Japanese"),
    ("3751989ca05e0454cbf35553ae20d6b2", "IBMPlexSansTC-Medium.otf", "Traditional Chinese"),
    ("b42279d3a9c9f1b4cada711297c04444", "IBMPlexSansKR-Medium.ttf", "Korean"),
)
UI_KEYS = {
    "language": "ui_setting_general_language_title",
    "start": "ui_pomodoro_setting_play",
    "pause": "ui_pomodoro_pause_title",
    "resume": "ui_pomodoro_using_unpause",
    "finish": "ui_pomodoro_using_reset",
    "statistics": "ui_statistics_title",
    "pomodoro": "ui_pomodoro_setting_preset_title",
    "countup": "ui_countup_setting_title",
    "working": "ui_pomodoro_work_title",
    "resting": "ui_pomodoro_break_title",
    "work": "ui_pomodoro_setting_work",
    "rest": "ui_pomodoro_setting_break",
    "loop": "ui_pomodoro_setting_loop",
}


def read_locale_ids(path):
    content = path.read_text(encoding="utf-8-sig")
    authored = {name: int(code) for name, code in re.findall(r"^\s*(\w+)\s*=\s*(\d+),?\s*$", content, re.M)}
    ids = {name: index for index, (name, _, _) in enumerate(LOCALES, start=1)}
    if authored != {"None": 0, **ids}:
        raise ValueError("Windows language selection IDs differ from the pinned source")
    return ids


def ui_strings(rows, keys=UI_KEYS.values()):
    selected = {}
    expected = set(keys)
    for row in rows:
        key = row["ID"]
        if key in expected:
            if key in selected:
                raise ValueError(f"Duplicate UI text ID: {key}")
            selected[key] = {field: row[field] for field in FIELDS}
            if any(not text for text in selected[key].values()):
                raise ValueError(f"Missing translation for UI text ID: {key}")
    if missing := expected - selected.keys():
        raise ValueError(f"Missing UI text IDs: {sorted(missing)}")
    return selected


def language_config(strings, default):
    entries = [f"    {name} = {localized_text_config(strings[key], FIELDS)}," for name, key in UI_KEYS.items()]
    return f"local room_language_config = {{ default = {default}, strings = {{\n" + "\n".join(entries) + "\n}}\n"


def stage_fonts(images, assets):
    image_root = images.resolve(strict=True).parent
    slots = json.loads(images.read_bytes())["slots"]
    faces, receipt = [], {}
    for guid, name, language in SOURCE_FONTS:
        item = slots[guid]
        source = (image_root / item["media"]).resolve(strict=True)
        if (source.parent != image_root / "media" or item["source"] != "Assets/Font/" + name
                or source.suffix != Path(name).suffix or digest(source) != item["sha256"]):
            raise ValueError(f"Language font identity differs: {name}")
        target = assets / name
        os.link(source, target)
        write(target.with_name(name + ".azmeta"), {"schemaVersion": 1, "id": str(UUID(hex=guid)),
              "settings": {"retainSourceSnapshot": False}, "dependencies": []})
        faces.append(f'@font-face {{ font-family: "Room UI {language}"; src: "{name}"; -rmlui-fallback-face: true; }}')
        receipt[language] = {"asset": name, "source": item["source"], "sha256": item["sha256"]}
    return "\n".join(faces) + "\n", receipt


def language_panel(base, template, ids, faces):
    style, options = template.split("</style>\n", 1)
    buttons = []
    for name, _, autonym in LOCALES:
        action = f"room-language-{ids[name]}"
        buttons.append(f'<button id="{action}" data-action="{action}">{escape(autonym)}</button>')
    if base.count("</style>") != 1 or base.count("</body>") != 1 or "room-language-panel" in base:
        raise ValueError("Expected the integrated room UI without a language panel")
    body = options.replace("<!-- room-language-options -->", "\n".join(buttons), 1)
    return base.replace("</style>", faces + style + "</style>", 1).replace("</body>", body + "</body>", 1)


def calendar_strings(rows):
    return ui_strings(rows, (*CALENDAR_WEEKDAYS, *CALENDAR_LABELS.values()))


def calendar_config(strings):
    if strings is None:
        return ""
    weekdays = ", ".join(localized_text_config(strings[key], FIELDS) for key in CALENDAR_WEEKDAYS)
    labels = ", ".join(f"{label} = {localized_text_config(strings[key], FIELDS)}"
                       for label, key in CALENDAR_LABELS.items())
    return f"local calendar_labels = {{ weekdays = {{{weekdays}}}, {labels} }}\n"


def localized_scripts(examples, strings, default, voice, choices, rewards, calendar=None):
    return [
        language_config(strings, default).encode("utf-8"),
        voice_config(voice["responses"], FIELDS).encode("utf-8"),
        choice_config(choices["episodes"], choices["parameters"], FIELDS).encode("utf-8"),
        *((examples / file).read_bytes() for file in ("room-language.lua", "room-voice.lua", "work-choice.lua")),
        calendar_config(calendar).encode("utf-8"),
        calendar_script(examples, rewards),
    ]


def stage_languages(room, source, inventory, images, output, language):
    enum = source / GAME_LANGUAGE
    ids = read_locale_ids(enum)
    default = FIELDS.index(language) + 1
    story_hash = digest(inventory)
    rows = json.loads(inventory.read_bytes())["localization"]
    strings = ui_strings(rows)
    calendar = calendar_strings(rows)
    voice = json.loads((room / "voice-receipt.json").read_bytes())
    choices = json.loads((room / "choice-receipt.json").read_bytes())
    if voice["storySHA256"] != story_hash or choices["storySHA256"] != story_hash:
        raise ValueError("Room speech and UI must share the pinned source inventory")
    if choices.get("seed") is not None:
        raise ValueError("Language settings require the production random sequence")
    rewards = json.loads((room / "work-progression-receipt.json").read_bytes())
    descriptor = json.loads((room / "project.azureproject").read_bytes())
    actions = {"room-language-toggle": [], **{f"room-language-{index}": [] for index in ids.values()}}
    if actions.keys() & descriptor["runtime"]["input"].keys():
        raise ValueError("The room already has a language selector")
    descriptor["runtime"]["input"].update(actions)
    examples = Path(__file__).resolve().parents[2] / "examples/npr-room/assets"
    scripts = localized_scripts(examples, strings, default, voice, choices, rewards, calendar)
    receipt = {"schemaVersion": 1, "storySHA256": story_hash, "enumSHA256": digest(enum),
               "default": default, "languages": dict(zip(FIELDS, ids.values())),
               "ui": strings, "calendar": calendar, "idleResponses": len(voice["responses"]),
               "workConversations": len(choices["episodes"])}
    with tempfile.TemporaryDirectory(prefix="room-language-", dir=output.parent) as holder:
        project = Path(holder) / "project"
        assets = project / "assets"
        shutil.copytree(room / "assets", assets, copy_function=copy_room_asset, ignore=shutil.ignore_patterns(".azure"))
        for item in room.glob("*-receipt.json"):
            shutil.copyfile(item, project / item.name)
        faces, receipt["fonts"] = stage_fonts(images, assets)
        ui = assets / "timer.rml"
        body = language_panel(ui.read_text(encoding="utf-8"),
                              (examples / "room-language.rml").read_text(encoding="utf-8"), ids, faces)
        ui.write_text(body, encoding="utf-8", newline="\n")
        (assets / "pomodoro.lua").write_bytes(b"\n".join(scripts))
        write(project / "project.azureproject", descriptor)
        write(project / "language-receipt.json", receipt)
        publish_directory(project, output)
    return receipt


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("room", "source", "inventory", "images", "output"):
        parser.add_argument("--" + argument, type=Path, required=True)
    parser.add_argument("--language", choices=FIELDS, default="ZhHans")
    args = parser.parse_args()
    data = stage_languages(args.room, args.source, args.inventory, args.images, args.output, args.language)
    print(json.dumps({key: data[key] for key in ("languages", "default", "idleResponses", "workConversations")}))
