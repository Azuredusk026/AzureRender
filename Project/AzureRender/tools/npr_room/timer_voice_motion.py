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


def resolve_motions(sources, clips, selected, sequences=None):
    samples = {clip["name"]: clip for clip in clips}
    sequences = sequences or {}
    if len(samples) != len(clips) or len(set(selected)) != len(selected):
        raise ValueError("Timer motion samples and selected identities must be unique")
    motions = {}
    for identity in selected:
        route = sources[str(identity)]["clips"]
        if len(route) != 1:
            sequence = sequences.get(identity)
            if sequence is None or sequence["sourceHashes"] != [source["sha256"] for source in route]:
                raise ValueError(f"Timer motion requires a sampled source transition chain: {identity}")
            name = sequence["clip"]
            clip = samples[name]
            cues = {str(event["cue"]): event["seconds"] for event in sequence["cues"]}
            if (not clip["human"] or clip["rate"] != 60 or not math.isfinite(clip["length"]) or clip["length"] <= 0
                    or len(cues) != len(sequence["cues"])
                    or any(not math.isfinite(seconds) or not 0 <= seconds <= clip["length"] for seconds in cues.values())
                    or not {str(cue) for cue in sources[str(identity)]["requiredCues"]} <= cues.keys()):
                raise ValueError(f"Invalid native transition timeline or voice cues: {identity}")
            motions[str(identity)] = {"name": name, "state": f"timer-motion-{identity}", "seconds": clip["length"],
                                      "sourceHashes": sequence["sourceHashes"], "cues": cues,
                                      "sampleCount": len(clip["frames"]), "sampleRate": clip["rate"]}
            continue
        source = route[0]
        if source["speed"] != 1 or source["loop"]:
            raise ValueError(f"Expected a single-play timer motion: {identity}")
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


def stage_timer_motions(room, output, source_catalog, sample_paths, rig_path, selected):
    catalog = json.loads(source_catalog.read_bytes())
    sets = [json.loads(gzip.decompress(path.read_bytes())) for path in sample_paths]
    reference_bones = next((item["controlBones"] for item in sets if "controlBones" in item), None)
    if any(item["version"] != sets[0]["version"] or item.get("controlBones", reference_bones) != reference_bones
           for item in sets):
        raise ValueError("Timer motion batches require the same native Avatar and Unity build")
    clips = [clip for item in sets for clip in item["clips"]]
    sequences = {item["sequence"]["motion"]: item["sequence"] for item in sets if "sequence" in item}
    motions = resolve_motions(catalog["motions"], clips, selected, sequences)
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
            channels = export_model(path, staged, rig, clips)
            os.replace(staged, path)
            sidecar = path.with_suffix(".glb.azmeta")
            metadata = json.loads(sidecar.read_bytes())
            metadata.setdefault("settings", {})["retainSourceSnapshot"] = False
            write(sidecar, metadata)
            baked[path.stem] = {"channels": channels, "sha256": digest(path)}
        for index, clip in enumerate(clips, start=len(animations)):
            motion, = [row for row in motions.values() if row["name"] == clip["name"]]
            motion["clipIndex"] = index
            graph["states"].append({"name": motion["state"], "clip": index, "loop": False})
        write(graph_path, graph)
        receipt = {"schemaVersion": 1, "sourceSHA256": digest(source_catalog), "samplesSHA256": digest(sample_paths[0]),
                   "rigSHA256": digest(rig_path), "unityVersion": sets[0]["version"],
                   "motions": motions, "parts": baked}
        if len(sample_paths) > 1:
            receipt["sampleBatches"] = [{"file": path.name, "sha256": digest(path)} for path in sample_paths]
        write(project / "timer-motion-receipt.json", receipt)
        publish_directory(project, output)
    return receipt


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("room", "output", "catalog", "samples", "rig"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--additional-samples", type=Path, action="append", default=[])
    parser.add_argument("--motion", type=int, required=True, action="append")
    args = parser.parse_args()
    receipt = stage_timer_motions(args.room, args.output, args.catalog, [args.samples, *args.additional_samples],
                                  args.rig, args.motion)
    print(json.dumps({"motions": receipt["motions"], "parts": len(receipt["parts"])}))
