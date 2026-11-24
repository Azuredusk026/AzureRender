"""Stage the opening screen move material and attach its story command."""

from tools.npr_room.build_city_window import verified_path
from tools.npr_room.build_story_signal_room import EXAMPLE, load
from tools.npr_room.opening_camera import use_presentation
from tools.npr_room.reference_inventory import digest
from tools.npr_room.windows_noise_probe import SCREEN_MOVE_TEXTURES


NODE = "opening-story-screen-move"
KIND = "azure.screen-move"


def bind(program, observer="nil"):
    anchor = "new_story_face(self, story_face_tracks)"
    if program.count(anchor) != 1:
        raise ValueError("Screen move requires the opening face presentation")
    if "function new_story_hold(" not in program:
        program = use_presentation(program, (EXAMPLE / "story-blackout.lua").read_text(encoding="utf-8").rstrip())
    setup = """(function()
        local target = assert(self:find("opening-story-screen-move"))
        local observe = OBSERVER
        return new_story_hold("DirectionScreenMove", function(ratio, _, seconds)
            target:set("azure.screen-move", "ratio", ratio)
            if observe then observe(ratio, seconds) end
        end, false)
    end)()"""
    return program.replace(anchor, anchor + ", " + setup.replace("OBSERVER", observer))


def room_inputs(source_lock, images):
    lock = load(source_lock)
    authored, = (row for row in lock["renderers"]["High"]["passes"] if row["shaderProgram"] == "screen-move")
    slots = {value["binding"]: value for value in authored["textures"].values()}
    if set(slots) != set(range(5)) or not slots[4]["runtime"]:
        raise ValueError("Screen move requires the original screen color binding")
    material, media = {}, {}
    for slot, (name, size, srgb, _, _) in enumerate(SCREEN_MOVE_TEXTURES):
        texture = slots[slot]
        if texture["srgb"] != srgb or (texture["width"], texture["height"]) != size:
            raise ValueError(f"Screen move texture shape or color differs: {name}")
        filename = f"opening-screen-move-{name}.png"
        material[name] = f"assets:/{filename}"
        media[filename] = verified_path(images, texture)
    for name in ("ratio", "speed", "unstable"):
        material[name] = authored["floats"]["_" + name.capitalize()]
    if material["ratio"] != 0:
        raise ValueError("Expected the inactive original screen move")
    node = {"id": NODE, "components": {KIND: {"type": KIND, "version": 1, "data": material}}}
    return node, media, {"schemaVersion": 1, "sourceLockSHA256": digest(source_lock),
                         "materialSHA256": authored["materialSHA256"],
                         "media": {name: digest(path) for name, path in media.items()}}
