"""Install one source-locked atmosphere into a registered diagnostic room."""

import copy
from hashlib import sha256
import json
from pathlib import Path
import shutil
import tempfile
from uuid import NAMESPACE_URL, uuid5

from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.reference_inventory import digest
from tools.npr_room.windows_room_fog import binding, capture_original_view


def hide_atmosphere_geometry(level, recipe):
    mapping = level["sceneImports"]["RoomScene"]["nodeMap"]
    hidden = {mapping[f"unity-{draw['transform']}"] for draw in recipe["base"]["drawables"].values()}
    hidden.update(mapping[f"unity-batch-{value}"] for value in (7292, 7866))
    city, = [resource["id"] for resource in level["resources"] if resource["asset"] == "assets:/window-city.glb"]
    city_node, = [node for node in level["nodes"] if node.get("resourceId") == city]
    hidden.add(city_node["id"])
    selected = [node for node in level["nodes"] if node["id"] in hidden]
    if len(hidden) != 6 or len(selected) != len(hidden):
        raise ValueError("Atmosphere replacement requires complete imported renderer identities")
    for node in selected:
        component = node["components"].setdefault("azure.renderable", {"type": "azure.renderable", "version": 1, "data": {}})
        component["data"]["visible"] = False
    return sorted(hidden)


def install(frame, bundle, assets, level):
    output = assets / "original-atmosphere"
    if output.exists():
        raise FileExistsError(output)
    raw = frame.read_bytes()
    record = json.loads(raw)
    identity = sha256(raw).hexdigest()
    reports = [(bundle / name).read_bytes() for name in (
        "composition.json", "sky-native.json", "cloud-native.json", "fog-native.json", "atmosphere-source-lock.json")]
    composition, sky, cloud, fog, recipe = map(json.loads, reports)
    recipe_checksum = sha256(reports[-1]).hexdigest()
    cloud_frame, = cloud["frames"]
    weather = fog["source"]["weather"]
    captures = (composition["capturedFrameSHA256"], sky["preparation"]["frameSHA256"],
                cloud_frame["inputs"]["capturedFrameSHA256"], weather["capturedFrameSHA256"],
                fog["source"]["captureCameraSHA256"])
    modes = (composition["mode"], sky["mode"], cloud["mode"], weather["mode"])
    clocks = (composition["seconds"], sky["preparation"]["sourceSeconds"], cloud_frame["inputs"]["seconds"], weather["seconds"])
    references = (sky["preparation"]["referenceSHA256"], cloud_frame["inputs"]["referenceSHA256"], weather["referenceSHA256"])
    if (record.get("metadataOnly", False) or any(value != identity for value in captures)
            or any(value != record["weather"] for value in modes)
            or any(value != record["simulationTime"] for value in clocks)
            or any(value != recipe["windowsReference"][record["weather"]]["sha256"] for value in references)
            or cloud["program"] != "cozy-cloud"
            or composition["skySHA256"] != sky["rawSHA256"]
            or composition["cloudSHA256"] != cloud_frame["rawSHA256"]):
        raise ValueError("Atmosphere must match one complete original frame and source recipe")
    camera = capture_original_view(record)
    if fog["source"]["camera"] != camera:
        raise ValueError("Atmosphere camera differs from the original frame")
    payloads = {"environment.hdr": composition["environmentSHA256"], "fog.azfog": fog["volume"]["volumeSHA256"]}
    for name, checksum in payloads.items():
        if digest(bundle / name) != checksum:
            raise ValueError(f"Atmosphere payload differs: {name}")
    candidate = copy.deepcopy(level)
    hidden = hide_atmosphere_geometry(candidate, recipe)
    candidate.setdefault("renderSettings", {}).update(skyMode=1, skyIntensity=1, skyRotation=0)
    candidate["viewFog"] = binding("assets:/original-atmosphere/fog.azfog", camera, True)
    with tempfile.TemporaryDirectory(prefix="original-atmosphere-", dir=assets) as temporary:
        staged = Path(temporary) / "original-atmosphere"
        staged.mkdir()
        for name, checksum in payloads.items():
            path = staged / name
            shutil.copyfile(bundle / name, path)
            if digest(path) != checksum:
                raise ValueError(f"Atmosphere changed during installation: {name}")
            metadata = {"schemaVersion": 1, "id": str(uuid5(NAMESPACE_URL, "original-atmosphere-v1:" + identity + ":" + name)),
                        "dependencies": [], "settings": {"retainSourceSnapshot": False}}
            path.with_name(path.name + ".azmeta").write_text(json.dumps(metadata) + "\n", encoding="utf-8")
        publish_directory(staged, output)
    level.clear()
    level.update(candidate)
    return {"schemaVersion": 1, "frameSHA256": identity, "weather": record["weather"],
            "sourceSeconds": record["simulationTime"], "environmentPath": "assets/original-atmosphere/environment.hdr",
            "environmentSHA256": payloads["environment.hdr"], "fogSHA256": payloads["fog.azfog"],
            "recipeSHA256": recipe_checksum, "hiddenRenderers": hidden,
            "scope": "single captured atmosphere with Player environment launch argument"}
