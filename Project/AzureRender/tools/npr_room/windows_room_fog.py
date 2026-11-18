"""Transfer original Direct3D fog depth slices to a fixed room camera."""

import json
import math
from struct import pack

import numpy as np

from tools.npr_room.reference_inventory import digest
from tools.npr_room.windows_sky_probe import stage_constants
from tools.npr_room.windows_weather_probe import (
    FAR, NEAR, fog_camera, prepare, validate_clip_planes, weather_fields,
)


WIDTH, HEIGHT, SLICES = 128, 72, 24
DEPTH_ENCODING = (1, 1, 1 / math.log2(1 + FAR))


def depth_samples(slices=SLICES, *, near=NEAR, far=FAR):
    validate_clip_planes(near, far)
    if type(slices) is not int or slices <= 0:
        raise ValueError("Fog volume requires a positive slice count")
    unit_depth = (np.arange(slices) + .5) / slices
    scale = 1 / math.log2(1 + far)
    return np.maximum(near, np.exp2(unit_depth / scale) - 1)


def capture_view(manifest):
    width, height = (manifest[k] for k in ("width", "height"))
    lens = manifest["cameraLens"]
    validate_clip_planes(lens["nearPlane"], lens["farPlane"])
    if (type(width) is not int or type(height) is not int or width <= 0 or height <= 0
            or width * HEIGHT != WIDTH * height or lens["projection"] != "perspective"
            or lens["offsetNdc"] != [0, 0] or not 0 < lens["verticalFovRadians"] < math.pi):
        raise ValueError("Expected the fixed room camera and 16:9 viewport")
    vectors = np.array([manifest[key] for key in ("qaCameraPosition", "qaCameraTarget", "cameraUp")])
    if vectors.shape != (3, 3) or not np.isfinite(vectors).all():
        raise ValueError("Room camera requires finite view vectors")
    return {"projection": "perspective", "position": manifest["qaCameraPosition"],
            "target": manifest["qaCameraTarget"], "up": manifest["cameraUp"],
            "fieldOfViewRadians": lens["verticalFovRadians"],
            "nearPlane": lens["nearPlane"], "farPlane": lens["farPlane"],
            "projectionOffset": [0, 0], "orthographicHeight": 5}


def capture_original_view(record):
    near, far = record["near"], record["far"]
    validate_clip_planes(near, far)
    matrix = np.array([[record["worldToCamera"][f"m{row}{column}"] for column in range(4)]
                       for row in range(4)])
    projection = np.array([[record["projection"][f"m{row}{column}"] for column in range(4)]
                           for row in range(4)])
    if (not np.isfinite(matrix).all() or not np.isfinite(projection).all()
            or not np.allclose(matrix[3], [0, 0, 0, 1], atol=1e-6)
            or not np.allclose(projection[3], [0, 0, -1, 0], atol=1e-6)
            or projection[0, 0] <= 0 or projection[1, 1] <= 0 or record["reversedZ"] is not True):
        raise ValueError("Original camera requires finite perspective and reversed depth")
    expected_projection = np.array([
        [projection[0, 0], 0, 0, 0], [0, projection[1, 1], 0, 0],
        [0, 0, -(far + near) / (far - near), -2 * near * far / (far - near)],
        [0, 0, -1, 0]])
    if not np.allclose(projection, expected_projection, atol=1e-6, rtol=1e-5):
        raise ValueError("Original camera projection differs from its clip planes")
    inverse = np.linalg.inv(matrix)
    if not np.allclose(inverse[:3, :3].T @ inverse[:3, :3], np.eye(3), atol=1e-5):
        raise ValueError("Original camera view axes must be orthonormal")
    axes = np.array([1, 1, -1])
    position = np.array([record["position"][axis] for axis in "xyz"])
    if not np.allclose(position, inverse[:3, 3], atol=1e-5):
        raise ValueError("Original camera position differs from its view matrix")
    position = position * axes
    target = position - inverse[:3, 2] * axes
    up = inverse[:3, 1] * axes
    if (record["height"] <= 0 or not math.isclose(
            projection[1, 1] / projection[0, 0], record["width"] / record["height"], rel_tol=1e-5)):
        raise ValueError("Original camera aspect differs from its viewport")
    return capture_view({"width": record["width"], "height": record["height"],
                         "qaCameraPosition": position.tolist(), "qaCameraTarget": target.tolist(),
                         "cameraUp": up.tolist(), "cameraLens": {
                             "projection": "perspective", "offsetNdc": [projection[0, 2], projection[1, 2]],
                             "nearPlane": record["near"], "farPlane": record["far"],
                             "verticalFovRadians": 2 * math.atan(1 / projection[1, 1])}})


def prepare_room(source, catalog, references, programs, assembly, previous_camera, output, mode, seconds,
                 *, original_frame=False):
    record = json.loads(previous_camera.read_bytes())
    view = capture_original_view(record) if original_frame else capture_view(record)
    near, far = view["nearPlane"], view["farPlane"]
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
                                      aspect=WIDTH / HEIGHT, near=near, far=far)
    original = json.loads(references.read_bytes())
    modes = original["modes"]
    values = original["windowsReference"][mode]["data"]
    fields = weather_fields(values, modes[mode], seconds, "fog", None) | matrices
    fields["_WorldSpaceCameraPos"] = position

    stages = json.loads((programs / "windows-atmosphere-programs.json").read_bytes())["programs"]["fog"]["stages"]
    view_constants = {"_ProjectionParams": (1, near, far, 1 / far), "unity_MatrixVP": projection}
    (output / "view-0.bin").write_bytes(stage_constants(stages["vertex"], "$Globals", view_constants))
    (output / "world-0.bin").write_bytes(stage_constants(stages["fragment"], "$Globals", fields))
    # MyCozyWeatherService anchors the room weather at its custom pivot.
    # This is the pivot translation in the settled original room log.
    pivot = (15, 0, 0)
    model = bytearray((output / "object.bin").read_bytes())
    model[48:60] = pack("<3f", *pivot)
    (output / "object.bin").write_bytes(model)
    distances = depth_samples(near=near, far=far)
    reversed_z = near * (far / distances - 1) / (far - near)
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
    far = camera["farPlane"]
    if not math.isfinite(far) or far <= 0:
        raise ValueError("Fog binding requires a finite positive far plane")
    return {"schemaVersion": 1, "asset": asset, "enabled": enabled, "captureView": camera,
            "projection": {"schemaVersion": 1, "extent": [WIDTH, HEIGHT], "pixelOrigin": "upper-left",
                           "framePhase": 0, "pixelCenterOffset": .5, "jitterRadius": 0,
                           "depthEncoding": (1, 1, 1 / math.log2(1 + far)),
                           "minimumViewDepth": 0, "nearFadeWidth": 1e-6}}
