"""Transfer original Direct3D fog depth slices to a fixed room camera."""

import json
import math
from struct import pack

import numpy as np

from tools.npr_room.reference_inventory import digest
from tools.npr_room.windows_sky_probe import stage_constants
from tools.npr_room.windows_weather_probe import FAR, NEAR, fog_camera, prepare, weather_fields


WIDTH, HEIGHT, SLICES = 128, 72, 24
DEPTH_ENCODING = (1, 1, 1 / math.log2(1 + FAR))


def depth_samples(slices=SLICES):
    unit_depth = (np.arange(slices) + .5) / slices
    return np.maximum(NEAR, np.exp2(unit_depth / DEPTH_ENCODING[2]) - 1)


def capture_view(manifest):
    width, height = (manifest[k] for k in ("width", "height"))
    lens = manifest["cameraLens"]
    if (width * HEIGHT != WIDTH * height or lens["projection"] != "perspective"
            or lens["offsetNdc"] != [0, 0] or not math.isclose(lens["nearPlane"], NEAR, abs_tol=1e-7)
            or lens["farPlane"] != FAR):
        raise ValueError("Expected the fixed room camera and 16:9 viewport")
    return {"projection": "perspective", "position": manifest["qaCameraPosition"],
            "target": manifest["qaCameraTarget"], "up": manifest["cameraUp"],
            "fieldOfViewRadians": lens["verticalFovRadians"], "nearPlane": NEAR, "farPlane": FAR,
            "projectionOffset": [0, 0], "orthographicHeight": 5}


def prepare_room(source, catalog, references, programs, assembly, previous_camera, output, mode, seconds):
    view = capture_view(json.loads(previous_camera.read_bytes()))
    receipt = prepare(source, catalog, references, programs, assembly, output, mode, "fog", seconds, None)
    axes = np.array([1, 1, -1])
    position = np.array(view["position"]) * axes
    target = np.array(view["target"]) * axes
    forward = target - position
    forward /= np.linalg.norm(forward)
    right = np.cross(view["up"] * axes, forward)
    right /= np.linalg.norm(right)
    up = np.cross(forward, right)
    projection, matrices = fog_camera(forward, up, position=position, vertical_fov=view["fieldOfViewRadians"],
                                      aspect=WIDTH / HEIGHT)
    original = json.loads(references.read_bytes())
    modes = original["modes"]
    values = original["windowsReference"][mode]["data"]
    fields = weather_fields(values, modes[mode], seconds, "fog", None) | matrices
    fields["_WorldSpaceCameraPos"] = position

    stages = json.loads((programs / "windows-atmosphere-programs.json").read_bytes())["programs"]["fog"]["stages"]
    view_constants = {"_ProjectionParams": (1, NEAR, FAR, 1 / FAR), "unity_MatrixVP": projection}
    (output / "view-0.bin").write_bytes(stage_constants(stages["vertex"], "$Globals", view_constants))
    (output / "world-0.bin").write_bytes(stage_constants(stages["fragment"], "$Globals", fields))
    # MyCozyWeatherService anchors the room weather at its custom pivot.
    # This is the pivot translation in the settled original room log.
    pivot = (15, 0, 0)
    model = bytearray((output / "object.bin").read_bytes())
    model[48:60] = pack("<3f", *pivot)
    (output / "object.bin").write_bytes(model)
    distances = depth_samples()
    reversed_z = NEAR * (FAR / distances - 1) / (FAR - NEAR)
    (output / "depth-slices.bin").write_bytes(np.asarray(reversed_z, dtype="<f4").tobytes())
    return {"weather": receipt, "camera": view, "pivot": pivot, "captureCameraSHA256": digest(previous_camera),
            "resolution": [WIDTH, HEIGHT], "depthSamplesMetres": distances.tolist()}


def pack_fog_volume(raw_capture, target, sample_count=SLICES):
    source = np.frombuffer(raw_capture, dtype="<f4").reshape(sample_count, HEIGHT, WIDTH, 4)
    alpha = source[..., 3:]
    if not np.all(np.isfinite(source)) or np.any((alpha < 0) | (alpha > 1)):
        raise ValueError("Invalid original depth slice color or opacity")
    transport = np.concatenate((source[..., :3] * alpha, 1 - alpha), axis=-1)
    if np.any(transport < 0) or np.any(transport > np.finfo(np.float16).max):
        raise ValueError("Original fog transport is outside the engine volume range")
    encoded = transport.astype("<f2")
    with target.open("xb") as volume:
        volume.write(b"AZFOG001" + pack("<3I", WIDTH, HEIGHT, sample_count))
        volume.write(encoded.tobytes())
    return {"volumeSHA256": digest(target), "maximumPackingError": float(np.max(np.abs(transport - encoded)))}


def binding(asset, camera, enabled):
    return {"schemaVersion": 1, "asset": asset, "enabled": enabled, "captureView": camera,
            "projection": {"schemaVersion": 1, "extent": [WIDTH, HEIGHT], "pixelOrigin": "upper-left",
                           "framePhase": 0, "pixelCenterOffset": .5, "jitterRadius": 0,
                           "depthEncoding": DEPTH_ENCODING, "minimumViewDepth": 0, "nearFadeWidth": 1e-6}}
