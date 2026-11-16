"""Stage sampled source actions for the timer voice cues on all character parts."""

import argparse
import gzip
import json
import math
import os
from pathlib import Path
import shutil
import tempfile

from tools.build_face_sdf import parse_glb
from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.humanoid_clip_export import export_model
from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest
from tools.npr_room.room_geometry import HEROINE_PARTS, copy_room_asset


def resolve_motions(sources, clips, selected):
    samples = {clip["name"]: clip for clip in clips}
    if len(samples) != len(clips) or len(set(selected)) != len(selected):
        raise ValueError("Timer motion samples and selected identities must be unique")
    motions = {}
    for identity in selected:
        route = sources[str(identity)]["clips"]
        if len(route) != 1 or route[0]["speed"] != 1 or route[0]["loop"]:
            raise ValueError(f"Timer motion needs a multi-stage animation route: {identity}")
        source = route[0]
        name = Path(source["clip"]).stem
        clip = samples[name]
        serialized_seconds = source["stop"] - source["start"]
        rate = clip["rate"]
        if not clip["human"] or not math.isfinite(rate) or rate <= 0:
            raise ValueError(f"Expected sampled Humanoid motion: {name}")
        # Unity reports an extra terminal interval for some inclusive source tracks.
        lengths = (serialized_seconds, serialized_seconds + 1 / rate)
        if not any(math.isclose(clip["length"], value, abs_tol=1e-5) for value in lengths):
            raise ValueError(f"Timer motion differs from the source clip time range: {name}")
        cues = {str(event["cue"]): event["seconds"] for event in source["voiceEvents"]}
        if len(cues) != len(source["voiceEvents"]):
            raise ValueError(f"Timer motion has repeated marker IDs: {name}")
        motions[str(identity)] = {"name": name, "state": f"timer-motion-{identity}", "seconds": clip["length"],
                                  "sha256": source["sha256"], "cues": cues, "sampleCount": len(clip["frames"]),
                                  "serializedSeconds": serialized_seconds, "sampleRate": rate}
    if {item["name"] for item in motions.values()} != samples.keys():
        raise ValueError("The sample set must match the selected source motions")
    return motions


def stage_timer_motions(room, output, source_catalog, sample_path, rig_path, selected):
    catalog = json.loads(source_catalog.read_bytes())
    samples = json.loads(gzip.decompress(sample_path.read_bytes()))
    motions = resolve_motions(catalog["motions"], samples["clips"], selected)
    rig = json.loads(rig_path.read_bytes())["nodes"]
    with tempfile.TemporaryDirectory(prefix="timer-voice-motion-", dir=output.parent) as container:
        temporary = Path(container)
        project = temporary / "project"
        assets = project / "assets"
        shutil.copytree(room / "assets", assets, copy_function=copy_room_asset, ignore=shutil.ignore_patterns(".azure"))
        for item in room.glob("*"):
            if item.is_file() and (item.name == "project.azureproject" or item.name.endswith("-receipt.json")):
                shutil.copyfile(item, project / item.name)
        models = assets / "heroine"
        graph_path = models / "humanoid-motion.json"
        graph = json.loads(graph_path.read_bytes())
        original_clips = tuple(state["clip"] for state in graph["states"])
        paths = [models / (part + ".glb") for part in HEROINE_PARTS]
        baked = {}
        animations = None
        for path in paths:
            document, _ = parse_glb(path)
            names = [clip["name"] for clip in document["animations"]]
            if animations is None:
                animations = names
            if names != animations or set(original_clips) != set(range(len(names))):
                raise ValueError("The room character parts must share one ordered body-motion library")
            staged = temporary / path.name
            channels = export_model(path, staged, rig, samples["clips"])
            os.replace(staged, path)
            sidecar = path.with_suffix(".glb.azmeta")
            metadata = json.loads(sidecar.read_bytes())
            metadata.setdefault("settings", {})["retainSourceSnapshot"] = False
            write(sidecar, metadata)
            baked[path.stem] = {"channels": channels, "sha256": digest(path)}
        for index, clip in enumerate(samples["clips"], start=len(animations)):
            motion, = [row for row in motions.values() if row["name"] == clip["name"]]
            motion["clipIndex"] = index
            graph["states"].append({"name": motion["state"], "clip": index, "loop": False})
        write(graph_path, graph)
        receipt = {"schemaVersion": 1, "sourceSHA256": digest(source_catalog), "samplesSHA256": digest(sample_path),
                   "rigSHA256": digest(rig_path), "unityVersion": samples["version"],
                   "motions": motions, "parts": baked}
        write(project / "timer-motion-receipt.json", receipt)
        publish_directory(project, output)
    return receipt


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("room", "output", "catalog", "samples", "rig"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--motion", type=int, required=True, action="append")
    args = parser.parse_args()
    receipt = stage_timer_motions(args.room, args.output, args.catalog, args.samples, args.rig, args.motion)
    print(json.dumps({"motions": receipt["motions"], "parts": len(receipt["parts"])}))
