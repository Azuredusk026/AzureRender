"""Install the room controls for display mode, resolution and topmost."""

import json

from tools.npr_room.build_story_signal_room import EXAMPLE
from tools.npr_room.build_voice_room import localized_text_config
from tools.npr_room.reference_inventory import digest
from tools.npr_room.room_geometry import activate_room_control
from tools.npr_room.room_language import FIELDS, ui_strings


DISPLAY_UI_KEYS = {
    "display": "ui_setting_graphics_title",
    "mode": "ui_setting_windowmode_title",
    "windowed": "ui_setting_windowmode_windowed",
    "borderless": "ui_setting_windowmode_fullscreen",
    "resolution": "ui_setting_window_resolution_title",
    "custom": "ui_setting_window_resolution_custom",
    "topmost": "ui_setting_general_alwaysontop_title",
    "on": "ui_global_on",
    "off": "ui_global_off",
    "activeRate": "ui_setting_framerate_active",
    "inactiveRate": "ui_setting_framerate_deactive",
}


def display_config(rows):
    strings = ui_strings(rows, DISPLAY_UI_KEYS.values())
    entries = [f"    {name} = {localized_text_config(strings[key], FIELDS)}," for name, key in DISPLAY_UI_KEYS.items()]
    return "local display_labels = {\n" + "\n".join(entries) + "\n}\n"


def display_localization(room, inventory):
    source_sha = digest(inventory)
    receipt = json.loads((room / "language-receipt.json").read_bytes())
    if receipt["storySHA256"] != source_sha:
        raise ValueError("Display settings must use the room's language inventory")
    return json.loads(inventory.read_bytes())["localization"]


def bind_display_language(program):
    if program.count("function new_room_language(config)") != 1:
        raise ValueError("Expected one original room language controller")
    return program + "\n" + (EXAMPLE / "window-settings-language.lua").read_text(encoding="utf-8")


def install(project, scene, document, prefix="", localization=None):
    actions = ("toggle", "mode", "resolution", "topmost", "topmost-menu", "frame-active", "frame-inactive")
    if any("display-" + action in project["runtime"]["input"] for action in actions):
        raise ValueError("Room already has display actions")
    activate_room_control(project, scene, "display", actions)
    script = prefix + "display.lua"
    scene["nodes"][-1]["components"]["azure.script"]["data"]["asset"] = "assets:/" + script
    component = "azure.window-presentation"
    scene["nodes"].append({"id": "room-display-window", "components": {
        component: {"type": component, "version": 1, "data": {"screenSleepMode": 1}}}})
    ui, = [node["components"]["azure.game-ui"]["data"] for node in scene["nodes"] if
           node["components"].get("azure.game-ui", {}).get("data", {}).get("asset") == "assets:/timer.rml"]
    path = prefix + "timer.rml"
    ui["asset"] = "assets:/" + path
    markup = (EXAMPLE / "window-settings.rml").read_text(encoding="utf-8")
    styles, body = markup.removeprefix("<style>\n").split("</style>\n")
    if document.count("</style>") != 1 or document.count("</body>") != 1:
        raise ValueError("Expected one room display insertion point")
    document = document.replace("</style>", styles + "</style>").replace("</body>", body + "</body>")
    controller = (EXAMPLE / "window-settings.lua").read_text(encoding="utf-8")
    if localization is not None:
        controller = display_config(localization) + controller
    return {script: controller, path: document}
