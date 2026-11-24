"""Bind the original opening color effect to room assets and scenario commands."""

import re

from tools.npr_room.build_city_window import verified_path
from tools.npr_room.build_story_signal_room import EXAMPLE, load
from tools.npr_room.opening_camera import use_presentation
from tools.npr_room.reference_inventory import digest


NODE = "opening-story-chromatic"
KIND = "azure.screen-chromatic"


def bind(program, observer="nil"):
    anchor = "new_story_face(self, story_face_tracks)"
    if program.count(anchor) != 1:
        raise ValueError("Chromatic effect requires the usual opening face presentation")
    if "function new_story_ratio(" not in program:
        noise = (EXAMPLE / "story-noise.lua").read_text(encoding="utf-8").rstrip()
        pattern = r"^function new_story_noise\(host\).*?^end(?=\n|$)"
        program, replacements = re.subn(pattern, lambda _: noise, program, flags=re.MULTILINE | re.DOTALL)
        if replacements != 1:
            raise ValueError("Expected the opening ratio controller")
    setup = """(function()
        local target = assert(self:find("opening-story-chromatic"))
        local observe = OBSERVER
        return new_story_ratio("DirectionChromaticAberration", function(ratio, seconds)
            target:set("azure.screen-chromatic", "ratio", ratio)
            if observe then observe(ratio, seconds) end
        end)
    end)()"""
    return use_presentation(program, "").replace(anchor, anchor + ", " + setup.replace("OBSERVER", observer))


def room_inputs(source_lock, images):
    locked = load(source_lock)
    authored, = [effect for effect in locked["renderers"]["High"]["passes"] if effect["shaderProgram"] == "chromatic"]
    slots = {value["binding"]: value for value in authored["textures"].values()}
    if set(slots) != {0, 1, 2} or not slots[2]["runtime"]:
        raise ValueError("Chromatic effect requires the original screen color binding")
    media, material = {}, {}
    for field, binding, size in (("noise", 0, (256, 256)), ("timeline", 1, (64, 1))):
        texture = slots[binding]
        if not texture["srgb"] or (texture["width"], texture["height"]) != size:
            raise ValueError(f"Chromatic texture shape or color differs: {field}")
        filename = "opening-chromatic-" + field + ".png"
        media[filename] = verified_path(images, texture)
        material[field] = "assets:/" + filename
    for field in ("ratio", "unstable", "filamentScale", "rage"):
        material[field] = authored["floats"]["_" + field[0].upper() + field[1:]]
    if material["ratio"] != 0:
        raise ValueError("Expected the inactive original room color effect")
    node = {"id": NODE, "components": {KIND: {"type": KIND, "version": 1, "data": material}}}
    return node, media, {"schemaVersion": 1, "sourceLockSHA256": digest(source_lock),
                         "materialSHA256": authored["materialSHA256"],
                         "media": {name: digest(path) for name, path in media.items()}}
