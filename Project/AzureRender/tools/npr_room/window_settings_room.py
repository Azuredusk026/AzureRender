"""Install the room controls for display mode, resolution and topmost."""

from tools.npr_room.build_story_signal_room import EXAMPLE
from tools.npr_room.room_geometry import activate_room_control


def install(project, scene, document, prefix=""):
    actions = ("toggle", "mode", "resolution", "topmost", "topmost-menu")
    if any("display-" + action in project["runtime"]["input"] for action in actions):
        raise ValueError("Room already has display actions")
    activate_room_control(project, scene, "display", actions)
    script = prefix + "display.lua"
    scene["nodes"][-1]["components"]["azure.script"]["data"]["asset"] = "assets:/" + script
    component = "azure.window-presentation"
    scene["nodes"].append({"id": "room-display-window", "components": {
        component: {"type": component, "version": 1, "data": {}}}})
    ui, = [node["components"]["azure.game-ui"]["data"] for node in scene["nodes"] if
           node["components"].get("azure.game-ui", {}).get("data", {}).get("asset") == "assets:/timer.rml"]
    path = prefix + "timer.rml"
    ui["asset"] = "assets:/" + path
    markup = (EXAMPLE / "window-settings.rml").read_text(encoding="utf-8")
    styles, body = markup.removeprefix("<style>\n").split("</style>\n")
    if document.count("</style>") != 1 or document.count("</body>") != 1:
        raise ValueError("Expected one room display insertion point")
    document = document.replace("</style>", styles + "</style>").replace("</body>", body + "</body>")
    return {script: (EXAMPLE / "window-settings.lua").read_text(encoding="utf-8"), path: document}
