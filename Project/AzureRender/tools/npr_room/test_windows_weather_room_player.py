"""Replay two original weather samples in the same Vulkan room and camera."""

import argparse
import json
import os
from pathlib import Path
import uuid

import numpy as np

from tools.npr_room.reference_inventory import digest
from tools.npr_room.test_windows_sky_environment_player import capture
from tools.npr_room.windows_room_fog import binding


def replay(player, room, resources, references, weather, output):
    atmosphere = json.loads(references.read_bytes())
    environment = weather / "composition"
    fog = weather / "room-fog"
    samples = json.loads((environment / "composition.json").read_bytes())
    volumes = json.loads((fog / "room-fog-volume.json").read_bytes())
    mode = samples[0]["mode"]
    output.mkdir()
    results = {}
    cases = (("control", 0, False), ("early", 0, True), ("early-repeat", 0, True), ("late", 1, True))
    for label, index, fog_enabled in cases:
        source = samples[index]
        volume = volumes[index]
        sky_hdr = environment / f"environment-{index}.hdr"
        fog_volume = fog / f"fog-{index}.azfog"
        assert source["mode"] == mode and source["seconds"] == volume["source"]["weather"]["seconds"]
        assert digest(sky_hdr) == source["environmentSHA256"] and digest(fog_volume) == volume["volumeSHA256"]
        name = f"weather-{mode.lower()}-{uuid.uuid4().hex[:8]}"
        owned = room / "assets" / (name + ".azfog")
        metadata = owned.with_name(owned.name + ".azmeta")
        if fog_enabled:
            if metadata.exists():
                raise FileExistsError(metadata)
            os.link(fog_volume, owned)

        def configure(scene):
            if fog_enabled:
                scene["viewFog"] = binding("assets:/" + owned.name, volume["source"]["camera"], True)

        folder = output / label
        try:
            image, checksum = capture(player, room, resources, sky_hdr, atmosphere, folder, name, 1,
                                      level_setup=configure)
        finally:
            if fog_enabled:
                owned.unlink()
                metadata.unlink(missing_ok=True)
        manifest = json.loads((folder / "frames/capture_manifest.json").read_bytes())
        assert manifest["viewFogMatch"] == ("matched" if fog_enabled else "disabled"), manifest["viewFogMatch"]
        results[label] = (image.astype(np.int16), checksum, manifest)

    control, early, late = (results[key][0] for key in ("control", "early", "late"))
    diff_with_fog = np.abs(early - control)
    diff_in_time = np.abs(late - early)
    visible = int(np.count_nonzero(np.any(diff_with_fog, axis=2)))
    temporal = int(np.count_nonzero(np.any(diff_in_time, axis=2)))
    assert visible > 1000 and temporal > 100, (visible, temporal)
    assert np.array_equal(early, results["early-repeat"][0]), "Repeated source resources changed the room frame"
    opaque_ui = np.s_[32:150, 877:1245]
    for image in (early, late):
        assert np.array_equal(control[opaque_ui], image[opaque_ui]), "Weather changed the opaque timer panel"
    keys = ("qaCameraPosition", "qaCameraTarget", "cameraLens", "cameraUp")
    assert all(result[2][key] == results["early"][2][key] for result in results.values() for key in keys)
    proof = {"mode": mode, "seconds": [sample["seconds"] for sample in samples],
             "gpu": results["early"][2]["gpu"], "playerSHA256": digest(player), "fogChangedPixels": visible,
             "temporalChangedPixels": temporal,
             "maxFogRGBDifference": int(diff_with_fog.max()), "maxTemporalRGBDifference": int(diff_in_time.max()),
             "matchedFogViews": 3, "repeatedFrameMatches": True, "opaqueUiChangedPixels": 0,
             "frameSHA256": {key: result[1] for key, result in results.items()},
             "environmentSHA256": [sample["environmentSHA256"] for sample in samples],
             "referenceScope": "fixed-camera source passes in the Vulkan room"}
    (output / "weather-room-replay.json").write_text(json.dumps(proof, indent=2) + "\n", encoding="utf-8")
    return proof


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("player", "room", "resource-root", "references", "weather", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(replay(args.player.resolve(), args.room.resolve(), args.resource_root.resolve(),
                            args.references.resolve(), args.weather.resolve(), args.output.resolve())))
