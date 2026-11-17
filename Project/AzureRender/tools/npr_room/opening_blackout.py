"""Bind the original blackout commands to an opening room presentation."""

from tools.npr_room.build_city_window import verified_path
from tools.npr_room.build_story_signal_room import EXAMPLE, load
from tools.npr_room.opening_camera import use_presentation
from tools.npr_room.reference_inventory import digest


NODE = "opening-story-blackout"
KIND = "azure.screen-blackout"


def bind(program, writer):
    anchor = "new_story_face(self, story_face_tracks))"
    if program.count(anchor) != 1:
        raise ValueError("Blackout requires the usual opening camera and face presentation")
    program = program.replace(anchor, anchor[:-1] + ", new_story_blackout(" + writer + "))")
    story = (EXAMPLE / "story-blackout.lua").read_text(encoding="utf-8").rstrip()
    return use_presentation(program, story)


def bind_room(program, observer="nil"):
    writer = """(function()
        local target = assert(self:find("opening-story-blackout"))
        local observe = OBSERVER
        return function(ratio, blind, seconds)
            target:set("azure.screen-blackout", "blind", blind)
            target:set("azure.screen-blackout", "ratio", ratio)
            if observe then observe(ratio, blind, seconds) end
        end
    end)()"""
    return bind(program, writer.replace("OBSERVER", observer))


def room_inputs(source_lock, images):
    lock = load(source_lock)
    authored, = [row for row in lock["renderers"]["High"]["passes"] if row["shaderProgram"] == "blackout"]
    slots = {value["binding"]: value for value in authored["textures"].values()}
    if not slots[0]["runtime"] or slots[1]["sha256"] != slots[2]["sha256"]:
        raise ValueError("Blackout room requires the original opaque-color and ramp bindings")
    media, material = {}, {}
    for param, binding, size in (("ramp", 1, (32, 32)), ("timeline", 3, (64, 1))):
        texture = slots[binding]
        if not texture["srgb"] or (texture["width"], texture["height"]) != size:
            raise ValueError(f"Blackout texture shape or color differs: {param}")
        filename = "opening-blackout-" + param + ".png"
        media[filename] = verified_path(images, texture)
        material[param] = "assets:/" + filename
    for name in ("ratio", "unstable", "length", "dotScale", "speed", "blindTop", "blindBottom"):
        material[name] = authored["floats"]["_" + name[0].upper() + name[1:]]
    if material["ratio"] != 0 or authored["floats"]["_UseBlind"] != 0:
        raise ValueError("Expected the inactive original room blackout")
    material["blind"] = False
    node = {"id": NODE, "components": {KIND: {"type": KIND, "version": 1, "data": material}}}
    return node, media, {"schemaVersion": 1, "sourceLockSHA256": digest(source_lock),
                         "materialSHA256": authored["materialSHA256"],
                         "media": {name: digest(path) for name, path in media.items()}}
