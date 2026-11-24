"""Prepare and verify Windows opening effect captures."""

import argparse
import gzip
from hashlib import sha256
import json
from pathlib import Path
import re
import shutil
import struct

import numpy as np
from PIL import Image

from tools.npr_room.build_city_window import verified_path
from tools.npr_room.opening_glitch_camera import camera_source as glitch_camera_source
from tools.npr_room.reference_inventory import digest


PROGRAM = "windows-opening-effects-programs.json"
BINDINGS = "windows-opening-effects-source-lock.json"
BACKGROUND = np.array([.25, .5, .75, 1], dtype=np.float32)
BLACKOUT_SAMPLES = [[9.302, 0, 0], [9.302, .5, 0], [9.802, .5, 0], [9.302, 1, 0], [9.302, .5, 1]]
CHROMATIC_SAMPLES = [[9.302, 0], [9.302, .2], [9.802, .2], [9.302, .5]]
SCREEN_MOVE_SAMPLES = [[9.302, 0], [9.302, .5], [9.802, .5], [9.302, 1]]
GLITCH_SAMPLES = [[9.302, 0], [9.302, .5], [9.802, .5], [9.302, .3]]
SCREEN_MOVE_TEXTURES = (
    ("bands", (1, 32), False, 0, (0, 0, 0)),
    ("instability", (64, 1), True, 1, (0, 0, 0)),
    ("flow", (32, 18), True, 0, (1, 1, 0)),
    ("timeline", (32, 1), True, 0, (0, 0, 0)),
)


def prepare(effects, catalog):
    bindings = json.loads((effects / BINDINGS).read_bytes())
    program_path = effects / PROGRAM
    if bindings["programsSHA256"] != digest(program_path):
        raise ValueError("Direct3D opening program receipt moved away from its source lock")
    programs = json.loads(program_path.read_bytes())["programs"]
    for stage in programs["noise"]["stages"].values():
        if digest(effects / stage["path"]) != stage["sha256"]:
            raise ValueError("Original noise bytecode differs")
    native, = (item for item in bindings["renderers"]["High"]["passes"] if item["shaderProgram"] == "noise")
    expected = {"srcBlend": 5, "destBlend": 10, "srcBlendAlpha": 1, "destBlendAlpha": 10,
                "blendOp": 0, "blendOpAlpha": 0, "colMask": 15}
    if native["renderState"]["blend"] != expected or native["floats"]["_Unstable"] != 0:
        raise ValueError("Original noise requires its authored blend and initial parameters")
    timeline, = native["textures"].values()
    if not timeline["srgb"] or timeline["binding"] != 0:
        raise ValueError("Original noise requires the sRGB texture at slot zero")
    png = verified_path(catalog, timeline)
    with Image.open(png) as image:
        if image.size != (64, 1):
            raise ValueError("The original noise timeline requires 64 color samples")
        with (effects / "noise-timeline.rgba").open("xb") as output:
            output.write(image.convert("RGBA").tobytes())


def prepare_blackout(effects, catalog, source, output):
    lock = json.loads((effects / BINDINGS).read_bytes())
    program_file = effects / PROGRAM
    if digest(program_file) != lock["programsSHA256"]:
        raise ValueError("Blackout bytecode receipt differs from the source lock")
    stages = json.loads(program_file.read_bytes())["programs"]["blackout"]["stages"]
    authored, = [item for item in lock["renderers"]["High"]["passes"] if item["shaderProgram"] == "blackout"]
    if authored["renderState"]["blend"] != {"srcBlend": 5, "destBlend": 10, "srcBlendAlpha": 1,
                                             "destBlendAlpha": 10, "blendOp": 0, "blendOpAlpha": 0, "colMask": 15}:
        raise ValueError("Unexpected original blackout blend")
    if digest(source / authored["material"]) != authored["materialSHA256"]:
        raise ValueError("Blackout material differs from the source lock")
    layout, = [buffer for buffer in stages["fragment"]["constantBuffers"] if buffer["binding"] == 1]
    offsets = {item["name"]: item["offset"] for item in layout["vectors"]}
    expected = ("_Length", "_Ratio", "_DotScale", "_Speed", "_Unstable", "_BlindTop", "_BlindBottom", "_UseBlind")
    if (layout["bytes"] != 80 or [offsets[name] for name in expected] != list(range(48, 80, 4))
            or authored["floats"]["_Ratio"] != 0 or authored["floats"]["_UseBlind"] != 0):
        raise ValueError("Unexpected original blackout material layout")
    data = struct.pack("<20f", *([0] * 12 + [authored["floats"][name] for name in expected]))
    slots = {value["binding"]: value for value in authored["textures"].values()}
    if set(slots) != {0, 1, 2, 3} or not slots[0]["runtime"] or slots[1]["sha256"] != slots[2]["sha256"]:
        raise ValueError("Unexpected blackout texture slots")
    output.mkdir()
    for stage in stages.values():
        bytecode = effects / stage["path"]
        if digest(bytecode) != stage["sha256"]:
            raise ValueError("Original blackout bytecode differs")
        shutil.copyfile(bytecode, output / bytecode.name)
    texture_inputs = {}
    for binding, name, size, filter_mode in ((1, "ramp", (32, 32), 0), (3, "timeline", (64, 1), 1)):
        texture = slots[binding]
        if not texture["srgb"] or not texture["source"].startswith("Assets/Texture2D/"):
            raise ValueError("Expected an original sRGB blackout texture")
        texture_file = verified_path(catalog, texture)
        settings = source / (texture["source"] + ".meta")
        values = dict(re.findall(r"^\s*(filterMode|wrap[UVW]):\s*(\d+)\s*$",
                                 settings.read_text(encoding="utf-8"), re.M))
        if values != {"filterMode": str(filter_mode), "wrapU": "0", "wrapV": "0", "wrapW": "0"}:
            raise ValueError(f"Blackout texture sampler differs: {name}")
        with Image.open(texture_file) as image:
            if image.size != size:
                raise ValueError(f"Blackout texture dimensions differ: {name}")
            target = output / f"blackout-{name}.rgba"
            target.write_bytes(image.convert("RGBA").tobytes())
        texture_inputs[name] = {"sourceSHA256": digest(texture_file), "settingsSHA256": digest(settings),
                                "rgbaSHA256": digest(target), "size": size, "filterMode": filter_mode}
    (output / "blackout-material.bin").write_bytes(data)
    return {"programSHA256": digest(program_file), "sourceLockSHA256": digest(effects / BINDINGS),
            "textures": texture_inputs, "materialSHA256": authored["materialSHA256"],
            "materialConstantsSHA256": digest(output / "blackout-material.bin")}


def verify_blackout(first, repeat):
    checksum = digest(first)
    if digest(repeat) != checksum:
        raise ValueError("Blackout source replays differ under identical inputs")
    capture = np.fromfile(first, dtype="<f4").reshape(len(BLACKOUT_SAMPLES), 180, 320, 4)
    if not np.isfinite(capture).all() or not np.array_equal(capture[0], np.broadcast_to(BACKGROUND, capture[0].shape)):
        raise ValueError("The original blackout requires a finite capture and unchanged zero-ratio frame")
    changed = np.count_nonzero(np.any(capture[..., :3] != BACKGROUND[:3], axis=3), axis=(1, 2)).tolist()
    time_changed = int(np.count_nonzero(np.any(capture[1] != capture[2], axis=2)))
    blind_changed = int(np.count_nonzero(np.any(capture[1] != capture[4], axis=2)))
    if min(changed[1:]) <= 0 or time_changed == 0 or blind_changed == 0:
        raise ValueError("The original blackout lost its ratio, time, or blind input")
    return {"size": [320, 180], "frames": BLACKOUT_SAMPLES, "captureSHA256": checksum,
            "changedPixels": changed, "timeChangedPixels": time_changed, "blindChangedPixels": blind_changed,
            "rgbRange": [[float(frame[..., :3].min()), float(frame[..., :3].max())] for frame in capture]}


def compare_blackout(reference, candidate, repeat):
    result = verify_blackout(candidate, repeat)
    raw = gzip.decompress(reference.read_bytes()) if reference.suffix == ".gz" else reference.read_bytes()
    original = np.frombuffer(raw, dtype="<f4").reshape(len(BLACKOUT_SAMPLES), 180, 320, 4)
    actual = np.fromfile(candidate, dtype="<f4").reshape(original.shape)
    if not np.isfinite(original).all() or not np.array_equal(original[[0, 3]], actual[[0, 3]]):
        raise ValueError("Blackout Vulkan control or fully opaque frame differs from the original")
    difference = np.abs(original - actual)
    return result | {
        "originalSHA256": sha256(raw).hexdigest(),
        "meanRgbAbsoluteError": difference[..., :3].mean(axis=(1, 2, 3)).tolist(),
        "maxAbsoluteError": difference.max(axis=(1, 2, 3)).tolist(),
        "differentPixelsAboveTwoThousandths": np.count_nonzero(
            np.any(difference > .002, axis=3), axis=(1, 2)).tolist(),
        "differentPixels": np.count_nonzero(np.any(difference > 1e-6, axis=3), axis=(1, 2)).tolist(),
    }


def chromatic_source():
    y, x = np.indices((180, 320))
    return np.stack(((x % 17) / 16, (y % 9) / 8, ((x + y) % 32) / 32,
                     np.ones_like(x)), axis=2).astype("<f4")


def prepare_chromatic(effects, catalog, source, output):
    lock = json.loads((effects / BINDINGS).read_bytes())
    program_file = effects / PROGRAM
    if digest(program_file) != lock["programsSHA256"]:
        raise ValueError("Chromatic bytecode receipt differs from the source lock")
    stages = json.loads(program_file.read_bytes())["programs"]["chromatic"]["stages"]
    authored, = [item for item in lock["renderers"]["High"]["passes"] if item["shaderProgram"] == "chromatic"]
    if authored["renderState"]["blend"] != {"srcBlend": 1, "destBlend": 0, "srcBlendAlpha": 1,
                                             "destBlendAlpha": 0, "blendOp": 0, "blendOpAlpha": 0, "colMask": 15}:
        raise ValueError("Unexpected original chromatic blend")
    if digest(source / authored["material"]) != authored["materialSHA256"]:
        raise ValueError("Chromatic material differs from the source lock")
    layout, = [buffer for buffer in stages["fragment"]["constantBuffers"] if buffer["binding"] == 1]
    offsets = {item["name"]: item["offset"] for item in layout["vectors"]}
    names = ("_Ratio", "_Unstable", "_FilamentScale", "_Rage")
    if layout["bytes"] != 48 or [offsets[name] for name in names] != [32, 36, 40, 44] or authored["floats"]["_Ratio"] != 0:
        raise ValueError("Unexpected original chromatic constant layout")
    slots = {value["binding"]: value for value in authored["textures"].values()}
    if set(slots) != {0, 1, 2} or not slots[2]["runtime"]:
        raise ValueError("Unexpected original chromatic texture slots")
    output.mkdir()
    for stage in stages.values():
        bytecode = effects / stage["path"]
        if digest(bytecode) != stage["sha256"]:
            raise ValueError("Original chromatic bytecode differs")
        shutil.copyfile(bytecode, output / bytecode.name)
    texture_inputs = {}
    for slot, label, size in ((0, "noise", (256, 256)), (1, "timeline", (64, 1))):
        texture = slots[slot]
        settings = source / (texture["source"] + ".meta")
        values = dict(re.findall(r"^\s*(filterMode|wrap[UVW]):\s*(\d+)\s*$",
                                 settings.read_text(encoding="utf-8"), re.M))
        if not texture["srgb"] or values != {"filterMode": "1", "wrapU": "0", "wrapV": "0", "wrapW": "0"}:
            raise ValueError(f"Chromatic texture sampler differs: {label}")
        png = verified_path(catalog, texture)
        target = output / f"chromatic-{label}.rgba"
        with Image.open(png) as image:
            if image.size != size:
                raise ValueError(f"Chromatic texture dimensions differ: {label}")
            target.write_bytes(image.convert("RGBA").tobytes())
        texture_inputs[label] = {"sourceSHA256": digest(png), "settingsSHA256": digest(settings),
                                 "rgbaSHA256": digest(target), "size": size}
    material = output / "chromatic-material.bin"
    material.write_bytes(struct.pack("<12f", *([0] * 8 + [authored["floats"][name] for name in names])))
    screen = output / "chromatic-source.rgba32f"
    screen.write_bytes(chromatic_source().tobytes())
    return {"programSHA256": digest(program_file), "sourceLockSHA256": digest(effects / BINDINGS),
            "textures": texture_inputs, "materialSHA256": authored["materialSHA256"],
            "materialConstantsSHA256": digest(material), "screenSourceSHA256": digest(screen)}


def verify_chromatic(first, repeat):
    checksum = digest(first)
    if checksum != digest(repeat):
        raise ValueError("Original chromatic captures differ under identical inputs")
    capture = np.fromfile(first, dtype="<f4").reshape(len(CHROMATIC_SAMPLES), 180, 320, 4)
    if not np.isfinite(capture).all() or not (capture[..., 3] == 1).all():
        raise ValueError("Chromatic capture changed alpha or produced a non-finite value")
    if not np.array_equal(capture[0], chromatic_source()):
        raise ValueError("Zero chromatic ratio changed the screen source")
    changes = np.count_nonzero(capture[1:, ..., :3] != capture[0, ..., :3], axis=(1, 2))
    time_changed = int(np.count_nonzero(np.any(capture[1] != capture[2], axis=2)))
    ratio_changed = int(np.count_nonzero(np.any(capture[1] != capture[3], axis=2)))
    if (changes == 0).any() or not time_changed or not ratio_changed:
        raise ValueError("Original chromatic effect lost a color, ratio, or time input")
    return {"size": [320, 180], "frames": CHROMATIC_SAMPLES, "captureSHA256": checksum,
            "changedChannelPixels": changes.tolist(), "timeChangedPixels": time_changed,
            "ratioChangedPixels": ratio_changed,
            "rgbRange": [[float(frame[..., :3].min()), float(frame[..., :3].max())] for frame in capture]}


def compare_chromatic(reference, candidate, repeat):
    result = verify_chromatic(candidate, repeat)
    original = gzip.decompress(reference.read_bytes()) if reference.suffix == ".gz" else reference.read_bytes()
    expected = sha256(original).hexdigest()
    if expected != result["captureSHA256"]:
        raise ValueError("Vulkan chromatic output differs from the original on the same adapter")
    return result | {"originalSHA256": expected, "matchingPixels": len(CHROMATIC_SAMPLES) * 320 * 180}


def screen_move_source():
    y, x = np.indices((180, 320))
    return np.stack((x / 512, y / 256, ((x + 5 * y) % 256) / 256,
                     np.ones_like(x)), axis=2).astype("<f4")


def prepare_screen_move(effects, catalog, source, output):
    lock = json.loads((effects / BINDINGS).read_bytes())
    program = effects / PROGRAM
    if digest(program) != lock["programsSHA256"]:
        raise ValueError("Screen move bytecode receipt differs from the source lock")
    stages = json.loads(program.read_bytes())["programs"]["screen-move"]["stages"]
    authored, = (item for item in lock["renderers"]["High"]["passes"] if item["shaderProgram"] == "screen-move")
    blend = {"srcBlend": 1, "destBlend": 0, "srcBlendAlpha": 1, "destBlendAlpha": 0,
             "blendOp": 0, "blendOpAlpha": 0, "colMask": 15}
    if authored["renderState"]["blend"] != blend or digest(source / authored["material"]) != authored["materialSHA256"]:
        raise ValueError("Original screen move material differs")
    layout, = (buffer for buffer in stages["fragment"]["constantBuffers"] if buffer["binding"] == 1)
    offsets = {item["name"]: item["offset"] for item in layout["vectors"]}
    settings = {"_Ratio": (32, 0), "_Speed": (80, .5), "_Unstable": (84, 0)}
    if layout["bytes"] != 96 or any(offsets[name] != offset or authored["floats"][name] != value
                                    for name, (offset, value) in settings.items()):
        raise ValueError("Unexpected screen move shader constants")
    material = bytearray(layout["bytes"])
    for offset, value in settings.values():
        struct.pack_into("<f", material, offset, value)
    slots = {value["binding"]: value for value in authored["textures"].values()}
    if set(slots) != set(range(5)) or not slots[4]["runtime"]:
        raise ValueError("Unexpected screen move texture slots")
    output.mkdir()
    for stage in stages.values():
        bytecode = effects / stage["path"]
        if digest(bytecode) != stage["sha256"]:
            raise ValueError("Original screen move bytecode differs")
        shutil.copyfile(bytecode, output / bytecode.name)
    textures = {}
    for slot, (name, size, srgb, filter_mode, wrap) in enumerate(SCREEN_MOVE_TEXTURES):
        texture = slots[slot]
        metadata = source / (texture["source"] + ".meta")
        actual = dict(re.findall(r"^\s*(filterMode|wrap[UVW]|sRGBTexture):\s*(\d+)\s*$",
                                 metadata.read_text(encoding="utf-8"), re.M))
        sampler = {"filterMode": str(filter_mode), "sRGBTexture": str(int(srgb))}
        sampler.update({f"wrap{axis}": str(mode) for axis, mode in zip("UVW", wrap)})
        if actual != sampler or texture["srgb"] != srgb:
            raise ValueError(f"Screen move texture sampler differs: {name}")
        png = verified_path(catalog, texture)
        target = output / f"screen-move-{name}.rgba"
        with Image.open(png) as image:
            if image.size != size:
                raise ValueError(f"Screen move texture dimensions differ: {name}")
            target.write_bytes(image.convert("RGBA").tobytes())
        textures[name] = {"binding": slot, "size": size, "sampler": sampler, "sourceSHA256": digest(png),
                          "settingsSHA256": digest(metadata), "rgbaSHA256": digest(target)}
    parameters = output / "screen-move-material.bin"
    parameters.write_bytes(material)
    screen = output / "screen-move-source.rgba32f"
    screen.write_bytes(screen_move_source().tobytes())
    return {"programSHA256": digest(program), "sourceLockSHA256": digest(effects / BINDINGS),
            "materialSHA256": authored["materialSHA256"], "materialConstantsSHA256": digest(parameters),
            "screenSourceSHA256": digest(screen), "textures": textures}


def verify_screen_move(first, repeat):
    checksum = digest(first)
    if checksum != digest(repeat):
        raise ValueError("Screen move captures changed under identical inputs")
    capture = np.fromfile(first, dtype="<f4").reshape(len(SCREEN_MOVE_SAMPLES), 180, 320, 4)
    source = screen_move_source()
    if not np.isfinite(capture).all() or not np.array_equal(capture[0], source):
        raise ValueError("Screen move requires a finite capture with a matching zero-ratio frame")
    coordinates = capture[..., :2] * [512, 256]
    if (coordinates < 0).any() or (coordinates >= [320, 180]).any():
        raise ValueError("Screen move sampled source outside the wrapped image")
    x, y = np.moveaxis(coordinates.astype(np.int64), -1, 0)
    if not np.array_equal(capture, source[y, x]):
        raise ValueError("Screen move changed sampled source colors")
    changed = np.count_nonzero(np.any(capture != source, axis=3), axis=(1, 2)).tolist()
    time_changed = np.count_nonzero(np.any(capture[1] != capture[2], axis=2))
    ratio_changed = np.count_nonzero(np.any(capture[1] != capture[3], axis=2))
    if min(changed[1:]) == 0 or not time_changed or not ratio_changed:
        raise ValueError("Screen move lost its ratio or time input")
    return {"size": [320, 180], "frames": SCREEN_MOVE_SAMPLES, "captureSHA256": checksum,
            "changedPixels": changed, "timeChangedPixels": int(time_changed), "ratioChangedPixels": int(ratio_changed)}


def compare_screen_move(reference, candidate, repeat):
    result = verify_screen_move(candidate, repeat)
    original = gzip.decompress(reference.read_bytes()) if reference.suffix == ".gz" else reference.read_bytes()
    expected = sha256(original).hexdigest()
    if expected != result["captureSHA256"]:
        reference_frames = np.frombuffer(original, dtype="<f4").reshape(len(SCREEN_MOVE_SAMPLES), 180, 320, 4)
        actual = np.fromfile(candidate, dtype="<f4").reshape(reference_frames.shape)
        counts = np.count_nonzero(np.any(actual != reference_frames, axis=3), axis=(1, 2)).tolist()
        raise ValueError(f"Vulkan screen move differs from the original on the same adapter: {counts} pixels")
    return result | {"originalSHA256": expected, "matchingPixels": len(SCREEN_MOVE_SAMPLES) * 320 * 180}


def glitch_screen_source():
    y, x = np.indices((288, 512))
    return np.stack((32 + x % 128, 48 + y % 144, 48 + (3 * x + 5 * y) % 144,
                     np.full_like(x, 255)), axis=2).astype(np.uint8)


def prepare_glitch(effects, catalog, source, output):
    locked_path = effects / BINDINGS
    lock = json.loads(locked_path.read_bytes())
    program = effects / PROGRAM
    if digest(program) != lock["programsSHA256"]:
        raise ValueError("Glitch bytecode receipt differs from the source lock")
    camera = glitch_camera_source(source, locked_path)
    target = camera["target"]
    if (target["size"], target["colorFormat"], target["srgb"], target["filter"], target["wrap"]) != (
            [512, 288], 8, False, 0, [1, 1, 1]):
        raise ValueError("Unexpected original glitch camera target")
    stages = json.loads(program.read_bytes())["programs"]["glitch"]["stages"]
    authored, = (item for item in lock["renderers"]["High"]["passes"] if item["shaderProgram"] == "glitch")
    blend = {"srcBlend": 5, "destBlend": 10, "srcBlendAlpha": 1, "destBlendAlpha": 10,
             "blendOp": 0, "blendOpAlpha": 0, "colMask": 15}
    if authored["renderState"]["blend"] != blend:
        raise ValueError("Unexpected original glitch blend")
    layout, = (buffer for buffer in stages["fragment"]["constantBuffers"] if buffer["binding"] == 1)
    offsets = {item["name"]: item["offset"] for item in layout["vectors"]}
    fields = {"_Length": 16, "_Ratio": 20, "_DotScale": 24, "_Speed": 28, "_Unstable": 48}
    if layout["bytes"] != 64 or any(offsets[name] != offset for name, offset in fields.items()):
        raise ValueError("Unexpected original glitch shader constants")
    material = bytearray(layout["bytes"])
    for name, offset in fields.items():
        struct.pack_into("<f", material, offset, authored["floats"][name])
    timeline, = (texture for texture in authored["textures"].values() if texture["binding"] == 0)
    if not (timeline["srgb"] and authored["textures"]["_RenderTexture"]["binding"] == 1):
        raise ValueError("Unexpected original glitch texture slots")
    png = verified_path(catalog, timeline)
    metadata = source / (timeline["source"] + ".meta")
    sampler = dict(re.findall(r"^\s*(filterMode|wrap[UVW]):\s*(\d+)\s*$",
                              metadata.read_text(encoding="utf-8"), re.M))
    if sampler != {"filterMode": "1", "wrapU": "0", "wrapV": "0", "wrapW": "0"}:
        raise ValueError("Unexpected original glitch timeline sampler")
    output.mkdir()
    for stage in stages.values():
        bytecode = effects / stage["path"]
        if digest(bytecode) != stage["sha256"]:
            raise ValueError("Original glitch bytecode differs")
        shutil.copyfile(bytecode, output / bytecode.name)
    timeline_path = output / "glitch-timeline.rgba"
    with Image.open(png) as image:
        if image.size != (64, 1):
            raise ValueError("Unexpected original glitch timeline dimensions")
        timeline_path.write_bytes(image.convert("RGBA").tobytes())
    constants = output / "glitch-material.bin"
    constants.write_bytes(material)
    screen = output / "glitch-source.rgba"
    screen.write_bytes(glitch_screen_source().tobytes())
    return {"programSHA256": digest(program), "camera": camera, "materialSHA256": authored["materialSHA256"],
            "materialConstantsSHA256": digest(constants), "screenSourceSHA256": digest(screen),
            "timelineSHA256": digest(png), "timelineRgbaSHA256": digest(timeline_path),
            "timelineSamplerSHA256": digest(metadata)}


def verify_glitch(first, repeat):
    checksum = digest(first)
    if checksum != digest(repeat):
        raise ValueError("Glitch captures changed under identical inputs")
    capture = np.fromfile(first, dtype="<f4").reshape(len(GLITCH_SAMPLES), 180, 320, 4)
    if not np.isfinite(capture).all() or not np.all(capture[..., 3] == 1):
        raise ValueError("Glitch capture produced invalid color or alpha")
    if not np.array_equal(capture[0], np.broadcast_to(BACKGROUND, capture[0].shape)):
        raise ValueError("The original glitch requires an unchanged zero-ratio frame")
    changed = np.count_nonzero(np.any(capture != BACKGROUND, axis=3), axis=(1, 2)).tolist()
    time_changed = int(np.count_nonzero(np.any(capture[1] != capture[2], axis=2)))
    ratio_changed = int(np.count_nonzero(np.any(capture[1] != capture[3], axis=2)))
    if not (min(changed[1:]) > 0 and time_changed and ratio_changed):
        raise ValueError("The original glitch lost its ratio or time input")
    return {"size": [320, 180], "frames": GLITCH_SAMPLES, "captureSHA256": checksum,
            "changedPixels": changed, "timeChangedPixels": time_changed, "ratioChangedPixels": ratio_changed,
            "rgbRange": [[float(frame[..., :3].min()), float(frame[..., :3].max())] for frame in capture]}


def compare_glitch(reference, candidate, repeat):
    result = verify_glitch(candidate, repeat)
    raw = gzip.decompress(reference.read_bytes()) if reference.suffix == ".gz" else reference.read_bytes()
    expected = np.frombuffer(raw, dtype="<f4").reshape(len(GLITCH_SAMPLES), 180, 320, 4)
    actual = np.fromfile(candidate, dtype="<f4").reshape(expected.shape)
    difference = np.abs(actual - expected)
    counts = np.count_nonzero(np.any(difference > 2e-7, axis=3), axis=(1, 2)).tolist()
    if any(counts):
        raise ValueError(f"Vulkan glitch differs from the original on the same adapter: {counts} pixels")
    return result | {"originalSHA256": sha256(raw).hexdigest(), "matchingPixels": len(GLITCH_SAMPLES) * 320 * 180,
                     "maxAbsoluteError": difference.max(axis=(1, 2, 3)).tolist()}


def frame_counts(frames):
    if not np.array_equal(frames[0], np.broadcast_to(BACKGROUND, frames[0].shape)):
        raise ValueError("Zero noise changed the control frame")
    if not np.isfinite(frames).all() or not np.allclose(frames[:, :, :, 3], 1, atol=1e-7, rtol=0):
        raise ValueError("Noise changed the target alpha or produced a non-finite value")
    expected = np.array([[.2375, .475, .7125], [.2875, .525, .7625]], dtype=np.float32)
    count = []
    for frame in frames[1:]:
        unique, pixels = np.unique(frame[..., :3].reshape(-1, 3), axis=0, return_counts=True)
        if unique.shape != expected.shape or not np.allclose(unique, expected, atol=2e-7, rtol=0):
            raise ValueError("Noise was not composited at the specified 0.05 ratio")
        count.append([int(value) for value in pixels])
    moving = int(np.count_nonzero(np.any(frames[1] != frames[2], axis=2)))
    if moving < 0.2 * 320 * 180:
        raise ValueError("Time input failed to advance the noise pattern")
    return count, moving


def load_capture(path):
    return np.fromfile(path, dtype="<f4").reshape(3, 180, 320, 4)


def verify(first, repeat):
    checksum = digest(first)
    if checksum != digest(repeat):
        raise ValueError("Original noise captures differ under identical inputs")
    count, moving = frame_counts(load_capture(first))
    return {"size": [320, 180], "frames": [[9.302, 0], [9.302, .05], [9.802, .05]],
            "pixelCounts": count, "timeChangedPixels": moving, "captureSHA256": checksum}


def compare(reference, candidate, repeat):
    original = load_capture(reference)
    actual = load_capture(candidate)
    count, moving = frame_counts(actual)
    if digest(candidate) != digest(repeat):
        raise ValueError("Vulkan noise capture changed under identical inputs")
    original_count, _ = frame_counts(original)
    difference = np.abs(original - actual)
    return {"originalSHA256": digest(reference), "vulkanSHA256": digest(candidate),
            "sourcePixelCounts": original_count, "vulkanPixelCounts": count, "vulkanTimeChangedPixels": moving,
            "differentPixels": np.count_nonzero(np.any(difference > 1e-6, axis=3), axis=(1, 2)).tolist(),
            "meanRgbError": difference[..., :3].mean(axis=(1, 2, 3)).tolist()}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="operation", required=True)
    options = {"prepare": ("effects", "catalog"), "verify": ("first", "repeat", "output"),
               "compare": ("reference", "candidate", "repeat", "output"),
               "prepare-blackout": ("effects", "catalog", "source", "output"),
               "verify-blackout": ("first", "repeat", "output"),
               "compare-blackout": ("reference", "candidate", "repeat", "output"),
               "prepare-chromatic": ("effects", "catalog", "source", "output"),
               "verify-chromatic": ("first", "repeat", "output"),
               "compare-chromatic": ("reference", "candidate", "repeat", "output"),
               "prepare-screen-move": ("effects", "catalog", "source", "output"),
               "verify-screen-move": ("first", "repeat", "output"),
               "compare-screen-move": ("reference", "candidate", "repeat", "output"),
               "prepare-glitch": ("effects", "catalog", "source", "output"),
               "verify-glitch": ("first", "repeat", "output"),
               "compare-glitch": ("reference", "candidate", "repeat", "output")}
    for operation, paths in options.items():
        command = subparsers.add_parser(operation)
        for path in paths:
            command.add_argument("--" + path, type=Path, required=True)
    args = parser.parse_args()
    if args.operation == "prepare":
        prepare(args.effects, args.catalog)
    elif args.operation.startswith("prepare-"):
        effect = args.operation.removeprefix("prepare-")
        prepare_effect = {"blackout": prepare_blackout, "chromatic": prepare_chromatic,
                          "screen-move": prepare_screen_move, "glitch": prepare_glitch}[effect]
        report = prepare_effect(args.effects, args.catalog, args.source, args.output)
        (args.output / (effect + "-source-receipt.json")).write_text(json.dumps(report, sort_keys=True) + "\n",
                                                                   encoding="utf-8", newline="\n")
    else:
        if args.operation == "verify-blackout":
            report = verify_blackout(args.first, args.repeat)
        elif args.operation == "verify-chromatic":
            report = verify_chromatic(args.first, args.repeat)
        elif args.operation == "verify-screen-move":
            report = verify_screen_move(args.first, args.repeat)
        elif args.operation == "compare-screen-move":
            report = compare_screen_move(args.reference, args.candidate, args.repeat)
        elif args.operation == "verify-glitch":
            report = verify_glitch(args.first, args.repeat)
        elif args.operation == "compare-glitch":
            report = compare_glitch(args.reference, args.candidate, args.repeat)
        elif args.operation == "compare-chromatic":
            report = compare_chromatic(args.reference, args.candidate, args.repeat)
        elif args.operation == "compare-blackout":
            report = compare_blackout(args.reference, args.candidate, args.repeat)
        elif args.operation == "verify":
            report = verify(args.first, args.repeat)
        else:
            report = compare(args.reference, args.candidate, args.repeat)
        with args.output.open("x", encoding="utf-8", newline="\n") as output:
            json.dump(report, output, sort_keys=True)
            output.write("\n")
        print(json.dumps(report))
