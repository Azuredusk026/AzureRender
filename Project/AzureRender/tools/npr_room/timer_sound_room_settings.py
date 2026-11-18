"""Bind the original seven-language timer sound controls to a room candidate."""

from html import escape
import json
from pathlib import Path
import re

from tools.npr_room.build_voice_room import localized_text_config
from tools.npr_room.reference_inventory import digest
from tools.npr_room.room_language import FIELDS, ui_strings


LABEL_KEYS = {
    "title": "ui_timersound_setting_title",
    "pomodoro": "ui_timersound_setting_pomodoro_subtitle",
    "countup": "ui_timersound_setting_countup_subtitle",
    "start": "ui_timersound_setting_startsound_title",
    "finish": "ui_timersound_setting_finishsound_title",
    "on": "ui_global_on",
    "off": "ui_global_off",
    "error": "ui_common_error_body",
}
ASSETS = Path(__file__).resolve().parents[2] / "examples/npr-room/assets"
GROUPS = ("start", "finish")
TABS = ("pomodoro", "countup")
ACTION_PREFIX = "timer-sound-"
TIMER_ANCHOR = 'local slot = "pomodoro-settings"\n'
STYLES = """
#timer-sound-toggle, #timer-sound-panel button { padding: 4dp 7dp; margin: 4dp 3dp 0 0; color: white; background-color: #34434f; }
#timer-sound-toggle:hover, #timer-sound-panel button:hover { background-color: #506472; }
#timer-sound-panel { position: absolute; top: 100%; right: 0; width: 250dp; padding: 10dp; margin-top: 4dp; background-color: #19232c; font-size: 13dp; }
#timer-sound-pomodoro, #timer-sound-countup { display: inline-block; white-space: nowrap; }
.timer-sound-row { margin-top: 8dp; }
.timer-sound-row input { width: 32dp; padding: 3dp; color: white; background-color: #273541; }
.timer-sound-label { display: block; }
.timer-sound-selection { display: inline-block; width: 6dp; height: 6dp; margin-right: 4dp; border-radius: 3dp; background-color: #98d2f1; }
#timer-sound-status { display: block; color: #f0c58e; }
"""


def controls(strings, locale):
    actions = {}

    def label(name):
        return escape(strings[LABEL_KEYS[name]][FIELDS[locale - 1]])

    def button(name, caption, selectable=False):
        action = ACTION_PREFIX + name
        actions[action] = []
        content = caption
        if selectable:
            content = (f'<span class="timer-sound-selection" id="{action}-active" style="display: none"></span>'
                       f'<span id="{action}-label">{caption}</span>')
        return f'<button id="{action}" data-action="{action}">{content}</button>'

    toggle = button("toggle", label("title"))
    tabs = "".join(button(name, label(name), selectable=True) for name in TABS)
    rows = []
    for group in GROUPS:
        identifier = ACTION_PREFIX + group
        label_text = f'<span id="{identifier}-label" class="timer-sound-label">{label(group)}</span>'
        choices = "".join(button(f"{group}-{index}", letter, selectable=True)
                          for index, letter in enumerate("ABC", start=1))
        mute = button(group + "-mute", label("on"))
        volume = identifier + "-volume"
        actions[volume] = []
        rows.append(
            f'<div class="timer-sound-row">{label_text}{choices}'
            f' <input id="{volume}" type="text" maxlength="3" data-action-on-blur="{volume}" value="50" /> % {mute}</div>'
        )
    markup = (toggle + '<div id="timer-sound-panel" style="display: none">' + tabs
              + "".join(rows) + '<span id="timer-sound-status"></span></div>')
    return actions, markup


def replace_once(document, anchor, replacement):
    if document.count(anchor) != 1:
        raise ValueError(f"Expected one timer sound insertion point for {anchor.strip()}")
    return document.replace(anchor, replacement)


def bind_program(program, config, locale):
    prefix, anchor, timer = program.rpartition(TIMER_ANCHOR)
    if not anchor:
        raise ValueError("Room does not contain the timer controller")
    cue = "local function playCue(id)\n"
    timer = replace_once(
        timer, cue, "local timerSounds, timerSoundMenu\n" + cue
        + "    if timerSounds:play(id, countupMode) then return end\n",
    )
    init = "\n    loadCountup()\n"
    timer = replace_once(
        timer, init, init + "    timerSounds = new_timer_sound_settings(self)\n"
        + "    timerSoundMenu = new_timer_sound_menu(self, timerSounds, function(value)\n"
        + f"        return roomLanguage and roomLanguage:text(value) or value[{locale}]\n"
        + "    end)\n",
    )
    updates = "\n    updateDisplay()\n"
    before, found, after = timer.rpartition(updates)
    if not found or "\nfunction update(dt)" not in before:
        raise ValueError("Room does not contain the timer update callback")
    timer = before + "\n    timerSoundMenu:update(countupMode, languageChanged)" + found + after
    parts = [
        config,
        (ASSETS / "timer-sound-settings.lua").read_text(encoding="utf-8"),
        (ASSETS / "timer-sound-menu.lua").read_text(encoding="utf-8"),
        prefix + anchor + timer,
    ]
    return "\n".join(parts)


def install(room, descriptor, assets, inventory):
    rows = json.loads(inventory.read_bytes())["localization"]
    fingerprint = digest(inventory)
    language_receipt = room / "language-receipt.json"
    locale = FIELDS.index("En") + 1
    if language_receipt.exists():
        language = json.loads(language_receipt.read_bytes())
        if language["storySHA256"] != fingerprint or language["default"] not in range(1, len(FIELDS) + 1):
            raise ValueError("The timer sound menu must use the room language inventory")
        locale = language["default"]
    strings = ui_strings(rows, LABEL_KEYS.values())
    actions, markup = controls(strings, locale)
    inputs = descriptor["runtime"]["input"]
    if actions.keys() & inputs.keys():
        raise ValueError("Room already contains timer sound controls")

    ui_path, lua_path = assets / "timer.rml", assets / "pomodoro.lua"
    ui = ui_path.read_text(encoding="utf-8")
    timer_badge, = re.findall(r'<span id="timer-settings">[^<]*</span>', ui)
    ui = replace_once(ui, timer_badge, timer_badge + "\n" + markup)
    ui = replace_once(ui, "</style>", STYLES + "</style>")
    labels = ",\n".join(f"    {name} = {localized_text_config(strings[key], FIELDS)}"
                        for name, key in LABEL_KEYS.items())
    program = bind_program(lua_path.read_text(encoding="utf-8"), "local timer_sound_labels = {\n" + labels + "\n}", locale)
    ui_path.write_text(ui, encoding="utf-8", newline="\n")
    lua_path.write_text(program, encoding="utf-8", newline="\n")
    inputs.update(actions)
    return {"localizationSHA256": fingerprint, "defaultLocale": FIELDS[locale - 1], "labelKeys": LABEL_KEYS,
            "controls": list(actions), "documentSHA256": digest(ui_path), "controllerSHA256": digest(lua_path)}
