"""Stage the source controller and Humanoid clips for an offline Unity trace."""

import argparse
import json
from pathlib import Path
import shutil

from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest


FIRST_CHAPTER = ("0", "1", "3", "5", "1000", "1003")
SAMPLER = Path(__file__).with_name("unity") / "StoryBodyControllerProbe.cs"


def stage(source, layout, catalog_path, graph_path, output):
    source = source.resolve(strict=True)
    graph = json.loads(graph_path.read_bytes())
    catalog = json.loads(catalog_path.read_bytes())
    if catalog["sourceSHA256"]["graph"] != digest(graph_path):
        raise ValueError("Controller index does not match the sampled clips")
    clips = {row["clip"] for number in FIRST_CHAPTER for row in catalog["motions"][number]["states"]}
    wanted = {graph["source"]: graph["sourceSHA256"],
              **{path: catalog["clips"][path]["sha256"] for path in sorted(clips)}}
    inputs = {}
    for name, sha in wanted.items():
        path = (source / name).resolve(strict=True)
        if not path.is_relative_to(source / "Assets") or digest(path) != sha:
            raise ValueError(f"Changed original controller input: {name}")
        meta = Path(str(path) + ".meta")
        inputs[name] = {"sha256": sha, "metaSHA256": digest(meta)}
    rig_file = layout / "rig.json"
    rig = json.loads(rig_file.read_bytes())
    avatar = rig["avatar"]
    if Path(avatar).name != avatar or Path(avatar).suffix != ".asset":
        raise ValueError("Expected a local Humanoid Avatar asset")
    avatar_file = layout / "Assets" / avatar
    base = {f"Assets/{avatar}": avatar_file, f"Assets/{avatar}.meta": Path(str(avatar_file) + ".meta"),
            "Assets/Editor/StoryBodyControllerProbe.cs": SAMPLER,
            "Packages/manifest.json": layout / "Packages/manifest.json",
            "ProjectSettings/ProjectVersion.txt": layout / "ProjectSettings/ProjectVersion.txt"}
    output.mkdir(exist_ok=False)
    for relative, path in base.items():
        target = output / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(path, target)
    for relative in inputs:
        for suffix in ("", ".meta"):
            target = output / (relative + suffix)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source / (relative + suffix), target)
    write(output / "story-body-probe.json", {"animatorNode": rig["animatorNode"], "nodes": rig["nodes"],
                                            "avatar": avatar, "controller": graph["source"]})
    receipt = {"schemaVersion": 1, "controllerIndexSHA256": digest(graph_path), "catalogSHA256": digest(catalog_path),
               "rigSHA256": digest(rig_file), "localInputsSHA256": {name: digest(p) for name, p in base.items()},
               "sourceAssets": inputs}
    write(output / "story-body-source-receipt.json", receipt)
    return len(clips)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for arg in ("source", "layout", "catalog", "graph", "output"):
        parser.add_argument("--" + arg, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps({"clips": stage(args.source, args.layout, args.catalog, args.graph, args.output)}))
