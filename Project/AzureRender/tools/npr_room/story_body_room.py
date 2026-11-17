"""Stage the original opening chapter motions on the current seven-part character."""

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


def read_samples(root, original):
    clips = {}
    batches = []
    compatible = None
    for counter in range(1, 7):
        sample = root / f"opening-motion-{counter}.json.gz"
        receipt = json.loads((root / f"opening-motion-{counter}-receipt.json").read_bytes())
        checksum = digest(sample)
        if checksum != receipt["sampleSHA256"]:
            raise ValueError(f"Original opening body sample changed: {sample.name}")
        data = json.loads(gzip.decompress(sample.read_bytes()))
        signature = (data["version"], data["controlBones"])
        if compatible is not None and compatible != signature:
            raise ValueError("Opening body motions require the same Unity build and Avatar bones")
        compatible = signature
        for clip in data["clips"]:
            name = clip["name"]
            source = original[name]
            seconds = source["stop"] - source["start"]
            if (name in clips or not clip["human"] or clip["rate"] != 60
                    or len(clip["frames"]) != receipt["frames"][name]
                    or not any(math.isclose(clip["length"], value, abs_tol=1e-5) for value in (seconds, seconds + 1 / 60))):
                raise ValueError(f"Original opening body motion does not match its native source: {name}")
            clips[name] = clip
        batches.append({"file": sample.name, "sha256": checksum})
    if clips.keys() != original.keys():
        raise ValueError("Opening body motions require all original source clips")
    return clips, batches, compatible[0]


def stage(room, output, catalog_path, samples, rig_path):
    catalog = json.loads(catalog_path.read_bytes())
    original = {Path(path).stem: item for path, item in catalog["clips"].items()}
    clips, batches, unity_version = read_samples(samples, original)
    rig = json.loads(rig_path.read_bytes())["nodes"]
    with tempfile.TemporaryDirectory(prefix="story-body-room-", dir=output.parent) as temporary:
        container = Path(temporary)
        project = container / "project"
        assets = project / "assets"
        shutil.copytree(room / "assets", assets, copy_function=copy_room_asset, ignore=shutil.ignore_patterns(".azure"))
        for item in room.iterdir():
            if item.is_file() and (item.name == "project.azureproject" or item.name.endswith("-receipt.json")):
                shutil.copyfile(item, project / item.name)
        folder = assets / "heroine"
        graph_path = folder / "humanoid-motion.json"
        graph = json.loads(graph_path.read_bytes())
        states = {row["name"] for row in graph["states"]}
        names = None
        parts = {}
        for part in HEROINE_PARTS:
            model = folder / (part + ".glb")
            document, _ = parse_glb(model)
            available = [clip["name"] for clip in document["animations"]]
            if names is None:
                names = available
            if available != names:
                raise ValueError("Opening body motions require a shared order on all seven character parts")
            pending = [clips[name] for name in sorted(clips.keys() - set(available))]
            target = container / model.name
            channels = export_model(model, target, rig, pending)
            os.replace(target, model)
            parts[part] = {"sha256": digest(model), "channels": channels}
        added = sorted(clips.keys() - set(names))
        library = names + added
        motions = {name: {"name": name, "clipIndex": library.index(name), "sourceSHA256": original[name]["sha256"],
                          "seconds": clip["length"], "sampleCount": len(clip["frames"])} for name, clip in clips.items()}
        for route in catalog["motions"].values():
            for state in route["states"]:
                name = "story-" + state["state"]
                if name in states:
                    continue
                clip = Path(state["clip"]).stem
                graph["states"].append({"name": name, "clip": motions[clip]["clipIndex"], "loop": original[clip]["loop"]})
                states.add(name)
        write(graph_path, graph)
        timer_path = project / "timer-motion-receipt.json"
        if timer_path.is_file():
            timer = json.loads(timer_path.read_bytes())
            for part, record in timer["parts"].items():
                if record["sha256"] != digest(room / "assets/heroine" / (part + ".glb")):
                    raise ValueError(f"Existing timer body clips changed: {part}")
                record["sha256"] = parts[part]["sha256"]
            write(timer_path, timer)
        receipt = {"schemaVersion": 1, "catalogSHA256": digest(catalog_path), "unityVersion": unity_version,
                   "rigSHA256": digest(rig_path), "sampleBatches": batches, "parts": parts, "motions": motions,
                   "addedClips": added, "reusedClips": sorted(set(motions) & set(names))}
        write(project / "opening-body-receipt.json", receipt)
        publish_directory(project, output)
    return receipt


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("room", "output", "catalog", "samples", "rig"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    result = stage(args.room, args.output, args.catalog, args.samples, args.rig)
    print(json.dumps({"clips": len(result["motions"]), "reusedClips": result["reusedClips"], "parts": len(result["parts"])}))
