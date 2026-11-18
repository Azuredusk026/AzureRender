"""Bind the source-locked glitch camera and material to the opening room."""

from tools.npr_room.build_city_window import verified_path
from tools.npr_room.build_story_signal_room import EXAMPLE, load
from tools.npr_room.opening_camera import use_presentation
from tools.npr_room.reference_inventory import digest


NODE = "opening-story-glitch"
KIND = "azure.screen-glitch"


def bind(program, observer="nil"):
    anchor = "new_story_face(self, story_face_tracks)"
    if program.count(anchor) != 1:
        raise ValueError("Glitch requires the opening face presentation")
    parts = ["story-glitch.lua"]
    if "function new_story_hold(" not in program:
        parts.insert(0, "story-blackout.lua")
    prefix = "\n\n".join((EXAMPLE / part).read_text(encoding="utf-8").rstrip() for part in parts)
    setup = """(function()
        local target = assert(self:find("opening-story-glitch"))
        local observe = OBSERVER
        return new_story_glitch(function(ratio, seconds)
            target:set("azure.screen-glitch", "ratio", ratio)
            if observe then observe("ratio", ratio, seconds) end
        end, function(enabled, seconds)
            target:set("azure.screen-glitch", "cameraActive", enabled)
            if observe then observe("camera", enabled, seconds) end
        end)
    end)()"""
    return use_presentation(program, prefix).replace(anchor, anchor + ", " + setup.replace("OBSERVER", observer))


def room_inputs(source_lock, images, camera_receipt):
    locked = load(source_lock)
    camera = load(camera_receipt)
    if camera["effectsSHA256"] != digest(source_lock):
        raise ValueError("Glitch camera receipt belongs to a different room effects lock")
    pose, target = camera["camera"], camera["target"]
    if (pose["initialActive"] or any(pose["localPosition"].values())
            or pose["localRotation"] != {"x": 0, "y": 0, "z": 0, "w": 1}
            or pose["antialiasing"] != 1 or not pose["postProcessing"] or not pose["renderShadows"]
            or target["size"] != [512, 288] or target["srgb"] or target["colorFormat"] != 8
            or target["filter"] != 0 or target["wrap"] != [1, 1, 1]
            or camera["controller"]["enabledIntervalSeconds"] != [.3, 1]):
        raise ValueError("Unexpected original opening glitch camera settings")
    authored, = (row for row in locked["renderers"]["High"]["passes"] if row["shaderProgram"] == "glitch")
    slots = {value["binding"]: value for value in authored["textures"].values()}
    if set(slots) != {0, 1} or not slots[1]["runtime"]:
        raise ValueError("Glitch requires the original camera render target binding")
    timeline = slots[0]
    if not timeline["srgb"] or (timeline["width"], timeline["height"]) != (64, 1):
        raise ValueError("Unexpected original glitch timeline")
    filename = "opening-glitch-timeline.png"
    source = verified_path(images, timeline)
    material = {"timeline": f"assets:/{filename}", "fieldOfView": pose["fieldOfView"],
                "nearPlane": pose["clips"][0], "farPlane": pose["clips"][1]}
    for field in ("ratio", "length", "dotScale", "speed", "unstable"):
        material[field] = authored["floats"]["_" + field[0].upper() + field[1:]]
    if material["ratio"] != 0:
        raise ValueError("Expected the inactive original room glitch")
    node = {"id": NODE, "components": {KIND: {"type": KIND, "version": 1, "data": material}}}
    return node, {filename: source}, {"schemaVersion": 1, "sourceLockSHA256": digest(source_lock),
                                     "cameraSHA256": digest(camera_receipt), "materialSHA256": authored["materialSHA256"],
                                     "media": {filename: digest(source)}}
