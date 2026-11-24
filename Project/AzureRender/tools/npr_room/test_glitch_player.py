"""Verify the independent glitch view in a native Player."""

import argparse
import ctypes
import json
import os
from pathlib import Path

import numpy as np
from PIL import Image

from tools.npr_room.opening_glitch_room import KIND, room_inputs
from tools.npr_room.reference_inventory import digest
from tools.npr_room.test_notes_player import playing
from tools.npr_room.test_story_player import wait_for_slot
from tools.test_screen_noise_player import capture, component, stage, write


def configure(node, media, ratio, active, freeze=False, freeze_after_update=False, **overrides):
    def install(assets, nodes):
        for name, source in media.items():
            os.link(source, assets / name)
        settings = node["components"][KIND]["data"] | {"ratio": ratio, "cameraActive": active} | overrides
        components = {KIND: component(KIND, settings)}
        if freeze:
            pause = 'self:set("azure.screen-glitch", "cameraActive", false)'
            if freeze_after_update:
                script = f"""local frames = 0
function update()
    frames = frames + 1
    if frames == 10 then self:set("azure.screen-glitch", "cameraActive", true) end
    if frames == 60 then
        {pause}
        self:write_save("glitch-pause", {{1, true}})
    end
end
"""
            else:
                script = f"function init() {pause} end\n"
            (assets / "freeze.lua").write_text(
                script, encoding="utf-8")
            components["azure.script"] = component("azure.script", {"asset": "assets:/freeze.lua"})
        nodes.append({"id": node["id"], "components": components})
    return install


def replay(player, resources, effects, images, camera, output):
    node, media, receipt = room_inputs(effects, images, camera)

    cases = {"control": None, "inactive": configure(node, media, 0, False),
             "active": configure(node, media, .5, True), "frozen": configure(node, media, .5, True, freeze=True)}
    captures = {name: capture(player, resources, output / name, None, configure=setup)
                for name, setup in cases.items()}
    marker = np.s_[15:45, 15:110]
    scene = np.s_[170:]
    active_changes, frozen_changes = [], []
    for index, (control, inactive, active, frozen) in enumerate(zip(*captures.values(), strict=True)):
        assert np.array_equal(control, inactive)
        for frame in (active, frozen):
            assert np.all(frame[marker] == [44, 121, 167]), "Glitch camera changed the opaque game UI"
        if index == 0:
            assert np.array_equal(active, frozen), "The camera must produce its first frame before freezing"
        active_changes.append(int(np.count_nonzero(np.any(active[scene] != control[scene], axis=2))))
        frozen_changes.append(int(np.count_nonzero(np.any(frozen[scene] != control[scene], axis=2))))
    assert min(active_changes) > 1000 and min(frozen_changes) > 1000, (active_changes, frozen_changes)
    proof = {"activeChangedPixels": active_changes, "frozenChangedPixels": frozen_changes,
             "defaultPreservesFrame": True, "opaqueUiChangedPixels": 0,
             "material": receipt, "playerSHA256": digest(player)}
    write(output / "glitch-camera-replay.json", proof)
    return proof


def resize_replay(player, resources, effects, images, camera, output):
    set_dpi = ctypes.WinDLL("user32", use_last_error=True).SetThreadDpiAwarenessContext
    set_dpi.argtypes = [ctypes.c_void_p]
    set_dpi.restype = ctypes.c_void_p
    previous = set_dpi(-4)
    if not previous:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        return resize_in_physical_pixels(player, resources, effects, images, camera, output)
    finally:
        assert set_dpi(previous)


def resize_in_physical_pixels(player, resources, effects, images, camera, output):
    node, media, _ = room_inputs(effects, images, camera)
    frames, client = {}, {}

    def ready(window, call):
        assert window.find()
        client.update(window=window, call=call)

    frozen = configure(node, media, .5, False, freeze=True, freeze_after_update=True, speed=0.0)
    for label, install in (("control", None), ("frozen", frozen)):
        folder = output / label
        project = stage(folder / "project", None, configure=install)
        with playing(player, project, resources, folder / "profile", folder / "run",
                     level_reference="assets:/screen.azurelevel", on_ready=ready) as (_, _, _, screenshot):
            if label == "frozen":
                wait_for_slot(folder / "profile", "glitch-pause", lambda value: value == [1, True], 12)
            frames[label] = []
            for step, size in (("initial", (1280, 720)), ("compact", (800, 450)), ("restored", (1280, 720))):
                window = client["window"]
                if window.extent() != list(size):
                    assert window.resize(*size)
                    client["call"]("wait-frames", frames=8)
                path = screenshot(step + ".png")
                with Image.open(path) as image:
                    assert image.size == size, (step, image.size)
                    frames[label].append(np.array(image.convert("RGB")))
        diagnostics = "\n".join((folder / "run" / name).read_text(encoding="utf-8")
                                for name in ("stdout.log", "stderr.log"))
        assert not any(marker in diagnostics for marker in ("VUID-", "Validation Error", "SYNC-HAZARD")), diagnostics

    changed = [int(np.count_nonzero(np.any(a != b, axis=2)))
               for a, b in zip(frames["control"], frames["frozen"], strict=True)]
    assert changed[0] > 1000, ("Fixture needs an initial paused image", changed)
    assert min(changed[1:]) > 1000, ("Resize discarded the paused image", changed)
    assert np.array_equal(frames["frozen"][0], frames["frozen"][2]), "The paused view changed after restoring its extent"
    proof = {"frozenChangedPixels": changed, "restoredFrameMatches": True,
             "clientPixels": [[1280, 720], [800, 450], [1280, 720]], "playerSHA256": digest(player)}
    write(output / "glitch-camera-resize.json", proof)
    return proof


def exit_after_preload(player, resources, effects, images, camera, output):
    node, media, _ = room_inputs(effects, images, camera)
    project = stage(output / "project", None, configure=configure(node, media, 0, False))
    assets = project.parent / "assets"
    startup = assets / "screen.azurelevel"
    candidate = json.loads(startup.read_bytes())
    candidate["id"] = "future"
    write(assets / "future.azurelevel", candidate)
    entry = candidate | {"id": "screen-noise", "nodes": candidate["nodes"][:1]}
    entry["nodes"].append({"id": "preload", "components": {
        "azure.script": component("azure.script", {"asset": "assets:/preload.lua"}),
    }})
    write(startup, entry)
    (assets / "preload.lua").write_text("""local next_level = "assets:/future.azurelevel"
local wrote = false
function init() self:preload_level(next_level) end
function update()
    if not wrote and self:level_ready(next_level) then
        self:write_save("glitch-preload", {1, true})
        wrote = true
    end
end
""", encoding="utf-8")
    profile, launch = output / "profile", output / "run"
    with playing(player, project, resources, profile, launch, level_reference="assets:/screen.azurelevel"):
        wait_for_slot(profile, "glitch-preload", lambda value: value == [1, True], 12)
    diagnostics = "\n".join((launch / name).read_text(encoding="utf-8") for name in ("stdout.log", "stderr.log"))
    assert not any(marker in diagnostics for marker in ("VUID-", "Validation Error", "SYNC-HAZARD")), diagnostics
    assert "Level committed: future" not in diagnostics
    proof = {"preparedGlitchCamera": True, "candidateCommitted": False, "cleanAllocatorExit": True,
             "playerSHA256": digest(player)}
    write(output / "glitch-camera-preload-exit.json", proof)
    return proof


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("player", "resource-root", "source-lock", "image-catalog", "camera", "output"):
        parser.add_argument("--" + name, required=True, type=Path)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--resize", action="store_true")
    mode.add_argument("--preload-exit", action="store_true")
    args = parser.parse_args()
    run = resize_replay if args.resize else exit_after_preload if args.preload_exit else replay
    print(json.dumps(run(args.player.resolve(), args.resource_root.resolve(), args.source_lock.resolve(),
                         args.image_catalog.resolve(), args.camera.resolve(), args.output.resolve())))
