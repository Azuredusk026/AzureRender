"""Stage the Recovery audio catalog with content-addressed local media."""

import argparse
from collections import defaultdict
from hashlib import file_digest
import json
from pathlib import Path
import shutil
import tempfile
import time

from reference_inventory import META_GUID, text


AUDIO_TYPE = "UnityEngine.AudioClip"
EXTENSIONS = {".ogg", ".wav"}


def source_media(project, name, expected_guid):
    path = (project / name).resolve(strict=True)
    if not path.is_relative_to(project / "Assets") or path.suffix.lower() not in EXTENSIONS:
        raise ValueError(f"Invalid audio source: {name}")
    meta = META_GUID.search(text(path.with_name(path.name + ".meta")))
    if not meta or meta[1].lower() != expected_guid:
        raise ValueError(f"Audio source metadata changed: {name}")
    with path.open("rb") as stream:
        header = stream.read(12)
        valid = (header.startswith(b"OggS") if path.suffix.lower() == ".ogg"
                 else header.startswith(b"RIFF") and header[8:] == b"WAVE")
        if not valid:
            raise ValueError(f"Invalid audio container: {name}")
        stream.seek(0)
        digest = file_digest(stream, "sha256").hexdigest()
    return path, digest


def plan_catalog_audio(project, report):
    project = project.resolve(strict=True)
    catalog = report["catalog"]
    assets = report["assets"]
    by_path = defaultdict(list)
    by_location = {}
    for index, location in enumerate(catalog["locations"]):
        if location["resourceType"].get("m_ClassName") != AUDIO_TYPE:
            continue
        internal_id = location["internalId"]
        candidates = catalog["assetCandidates"].get(internal_id, {}).get("paths", [])
        if len(candidates) != 1:
            raise ValueError(f"Audio location needs a unique source: {internal_id}")
        path = candidates[0]
        by_path[path].append(internal_id)
        by_location[index] = assets[path]

    scene_paths = {path for path in report["sceneClosure"] if Path(path).suffix.lower() in EXTENSIONS}
    scene_only = scene_paths - by_path.keys()
    slots, payloads = {}, {}
    source_bytes = 0
    for name in sorted(set(by_path) | scene_paths):
        guid = assets[name]
        path, sha256 = source_media(project, name, guid)
        size = path.stat().st_size
        source_bytes += size
        payload_name = f"{sha256}{path.suffix.lower()}"
        payloads.setdefault(payload_name, path)
        if guid in slots:
            raise ValueError(f"Duplicate audio GUID: {guid}")
        slots[guid] = {
            "source": name,
            "sha256": sha256,
            "media": f"media/{payload_name}",
            "bytes": size,
            "catalogLocations": sorted(set(by_path.get(name, ()))),
            "sceneReference": name in scene_paths,
        }
    aliases = []
    for key in catalog["keys"]:
        hits = [by_location[i] for i in key["locations"] if i in by_location]
        if hits:
            aliases.append({
                "kind": key["kind"], "value": key["value"], "slots": list(dict.fromkeys(hits)),
            })
    summary = {
        "catalogLocations": len(by_location),
        "sceneOnlySources": len(scene_only),
        "sourceSlots": len(slots),
        "uniquePayloads": len(payloads),
        "sourceBytes": source_bytes,
        "payloadBytes": sum(path.stat().st_size for path in payloads.values()),
    }
    mapping = {
        "formatVersion": 1,
        "sourceCatalogSHA256": catalog["sha256"],
        "slots": slots,
        "aliases": aliases,
        "summary": summary,
    }
    return mapping, payloads


def publish_directory(staged, output):
    deadline = time.monotonic() + 2
    while True:
        if output.exists():
            raise FileExistsError(output)
        try:
            staged.rename(output)
            return
        except PermissionError as error:
            if getattr(error, "winerror", None) not in (5, 32, 33) or time.monotonic() >= deadline:
                raise
            time.sleep(0.05)


def stage_catalog_audio(project, report, output):
    if output.exists():
        raise FileExistsError(output)
    mapping, payloads = plan_catalog_audio(project, report)
    with tempfile.TemporaryDirectory(prefix="recovery-audio-", dir=output.parent) as directory:
        staged = Path(directory) / "publish"
        media = staged / "media"
        media.mkdir(parents=True)
        for payload, source in payloads.items():
            target = media / payload
            shutil.copyfile(source, target)
            with target.open("rb") as stream:
                actual = file_digest(stream, "sha256").hexdigest()
            if actual != target.stem:
                raise ValueError(f"Audio changed during staging: {source.name}")
        (staged / "audio-catalog.json").write_text(
            json.dumps(mapping, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n",
            encoding="utf-8", newline="\n",
        )
        publish_directory(staged, output)
    return mapping["summary"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--inventory", type=Path, required=True)
    parser.add_argument("--output", type=Path, help="New directory for the local staged catalog")
    args = parser.parse_args()
    inventory = json.loads(args.inventory.read_text(encoding="utf-8"))
    stats = (stage_catalog_audio(args.source, inventory, args.output) if args.output
             else plan_catalog_audio(args.source, inventory)[0]["summary"])
    print(json.dumps(stats))


if __name__ == "__main__":
    main()
