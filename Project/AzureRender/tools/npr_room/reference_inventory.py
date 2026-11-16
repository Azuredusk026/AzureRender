"""Record the static Unity reference inputs for the local room port."""

from base64 import b64decode
from collections import Counter, defaultdict, deque
from hashlib import file_digest
from pathlib import Path, PurePosixPath
from struct import iter_unpack, unpack_from
import argparse
import json
import re


GUID = r"[a-fA-F0-9]{32}"
META_GUID = re.compile(rf"(?m)^guid: ({GUID})\s*$")
BUILD_SCENE = re.compile(
    rf"(?m)^  - enabled: ([01])\r?\n    path: ([^\r\n]+)\r?\n    guid: ({GUID})\s*$"
)
OBJECT_REF = re.compile(rf"{{\s*fileID:\s*(-?\d+),\s*guid:\s*({GUID}),\s*type:\s*(\d+)\s*}}")
BUILTIN_GUIDS = {
    "0" * 32,
    "0000000000000000d000000000000000",
    "0000000000000000e000000000000000",
    "0000000000000000f000000000000000",
}


def text(path):
    return path.read_text(encoding="utf-8-sig")


def digest(path):
    with path.open("rb") as stream:
        return file_digest(stream, "sha256").hexdigest()


def asset_index(project):
    assets = {}
    for meta in sorted((project / "Assets").rglob("*.meta")):
        payload = meta.with_suffix("")
        if not payload.is_file():
            continue
        match = META_GUID.search(text(meta))
        if not match:
            raise ValueError(f"Missing asset GUID: {meta.relative_to(project)}")
        assets[payload.relative_to(project).as_posix()] = match[1].lower()
    return assets


def serialized_references(project, name):
    asset = project / name
    sources = [asset.with_name(asset.name + ".meta")]
    with asset.open("rb") as stream:
        if stream.read(12).removeprefix(b"\xef\xbb\xbf").startswith(b"%YAML"):
            sources.append(asset)
    return [
        {"guid": guid, "fileID": file_id, "type": reference_type}
        for guid, file_id, reference_type in sorted({
            (guid.lower(), int(file_id), int(reference_type))
            for source in sources
            for file_id, guid, reference_type in OBJECT_REF.findall(text(source))
            if int(file_id) != 0
        })
    ]


def walk_references(project, roots, by_guid):
    visited = set()
    references = {}
    unresolved = defaultdict(set)
    queue = deque(roots)
    while queue:
        name = queue.popleft()
        if name in visited:
            continue
        visited.add(name)
        references[name] = serialized_references(project, name)
        for ref in references[name]:
            guid = ref["guid"]
            matches = by_guid.get(guid, [])
            if len(matches) == 1:
                queue.append(matches[0])
            elif guid not in BUILTIN_GUIDS:
                unresolved[guid].add(name)
    return sorted(visited), references, {
        guid: sorted(names) for guid, names in sorted(unresolved.items())
    }


def resource_names(assets):
    names = defaultdict(list)
    for path in assets:
        parts = PurePosixPath(path).parts
        if "Resources" in parts[:-1]:
            index = parts.index("Resources")
            key = PurePosixPath(*parts[index + 1:]).with_suffix("").as_posix()
            names[key].append(path)
    return dict(sorted(names.items()))


def static_inventory(project):
    project = Path(project)
    settings = project / "ProjectSettings/EditorBuildSettings.asset"
    assets = asset_index(project)
    by_guid = group_paths_by_guid(assets)

    scenes = [
        {"path": path, "guid": guid.lower()}
        for enabled, path, guid in BUILD_SCENE.findall(text(settings))
        if enabled == "1"
    ]
    if not scenes:
        raise ValueError("Build settings contain no enabled scenes")
    for scene in scenes:
        if assets.get(scene["path"]) != scene["guid"]:
            raise ValueError(f"Build scene GUID differs from metadata: {scene['path']}")

    closure, references, unresolved = walk_references(
        project, [scene["path"] for scene in scenes], by_guid
    )
    return {
        "scenes": scenes,
        "sceneClosure": closure,
        "references": references,
        "unresolvedGuids": unresolved,
        "duplicateGuids": {guid: paths for guid, paths in by_guid.items() if len(paths) > 1},
        "assets": assets,
        "resources": resource_names(assets),
    }


def windows_identity(recovery):
    build = recovery / "Build"
    exe = build / "ChillWithYouRecovery.exe"
    data = build / "ChillWithYouRecovery_Data"
    files = [exe, build / "UnityPlayer.dll", build / "steam_appid.txt"]
    files.extend(p for p in data.rglob("*") if p.is_file())
    if not data.is_dir():
        raise ValueError(f"Missing Windows player data: {data}")
    return {
        path.relative_to(build).as_posix(): {"bytes": path.stat().st_size, "sha256": digest(path)}
        for path in sorted(files)
    }


def catalog_key(data, offset):
    if not 4 <= offset < len(data):
        raise ValueError("Addressables key offset is out of range")
    kind = data[offset]
    offset += 1
    if kind in (0, 1):
        size = unpack_from("<i", data, offset)[0]
        offset += 4
        if size < 0 or offset + size > len(data):
            raise ValueError("Addressables string length is out of range")
        encoding = "ascii" if kind == 0 else "utf-16-le"
        return kind, data[offset:offset + size].decode(encoding)
    if kind in (2, 3, 4):
        return kind, unpack_from(("<H", "<I", "<i")[kind - 2], data, offset)[0]
    if kind == 5 and data[offset:offset + 1] == b"\x20":
        return kind, data[offset + 1:offset + 33].decode("ascii")
    raise ValueError(f"Unsupported Addressables key type: {kind}")


def catalog_locations(catalog):
    """Read the compact catalog's key buckets and seven-column locations."""
    key_data = b64decode(catalog["m_KeyDataString"], validate=True)
    buckets = b64decode(catalog["m_BucketDataString"], validate=True)
    entry_data = b64decode(catalog["m_EntryDataString"], validate=True)
    count = unpack_from("<i", entry_data)[0]
    if count < 0 or len(entry_data) != 4 + count * 28:
        raise ValueError("Invalid Addressables location count")
    key_count = unpack_from("<i", buckets)[0]
    if key_count != unpack_from("<i", key_data)[0] or key_count < 0:
        raise ValueError("Invalid Addressables key count")

    keys = []
    offset = 4
    for _ in range(key_count):
        key_offset, size = unpack_from("<ii", buckets, offset)
        offset += 8
        if size < 0 or offset + 4 * size > len(buckets):
            raise ValueError("Invalid Addressables bucket size")
        indices = list(unpack_from(f"<{size}i", buckets, offset))
        offset += size * 4
        if any(index < 0 or index >= count for index in indices):
            raise ValueError("Addressables bucket location index is out of range")
        kind, value = catalog_key(key_data, key_offset)
        keys.append({"kind": kind, "value": value, "locations": indices})
    if offset != len(buckets):
        raise ValueError("Unexpected trailing Addressables bucket data")

    ids, providers = catalog["m_InternalIds"], catalog["m_ProviderIds"]
    types = catalog["m_resourceTypes"]
    locations = []
    for internal_id, provider, dep, _, _, primary, resource_type in iter_unpack("<7i", entry_data[4:]):
        if not (0 <= internal_id < len(ids) and 0 <= provider < len(providers)
                and -1 <= dep < key_count and 0 <= primary < key_count
                and 0 <= resource_type < len(types)):
            raise ValueError("Addressables location metadata index is out of range")
        locations.append({
            "internalId": ids[internal_id],
            "provider": providers[provider],
            "primaryKey": primary,
            "dependencyKey": None if dep == -1 else dep,
            "resourceType": types[resource_type],
        })
    return keys, locations

def locate_catalog_assets(internal_ids, assets, resources):
    stems = defaultdict(list)
    for path in assets:
        stems[PurePosixPath(path).with_suffix("").as_posix()].append(path)
    folded_resources = defaultdict(list)
    for key, paths in resources.items():
        folded_resources[key.casefold()].extend(paths)
    matches = {}
    for name in internal_ids:
        if name in assets:
            matches[name] = {"basis": "exact", "paths": [name]}
        elif name in resources:
            matches[name] = {"basis": "resources-key", "paths": resources[name]}
        elif name.casefold() in folded_resources:
            matches[name] = {"basis": "resources-key-casefold", "paths": folded_resources[name.casefold()]}
        elif f"Assets/{name}.unity" in assets:
            matches[name] = {"basis": "build-scene", "paths": [f"Assets/{name}.unity"]}
        elif name.startswith("Assets/"):
            candidates = stems.get(PurePosixPath(name).with_suffix("").as_posix())
            if candidates:
                matches[name] = {"basis": "exported-stem", "paths": candidates}
    return matches

def catalog_inventory(project, recovery, assets, resources):
    path = project / "Assets/StreamingAssets/aa/catalog.json"
    built = recovery / "Build/ChillWithYouRecovery_Data/StreamingAssets/aa/catalog.json"
    catalog = json.loads(text(path))
    keys, locations = catalog_locations(catalog)
    sha256 = digest(path)
    if catalog.get("m_InternalIdPrefixes"):
        raise ValueError("Addressables internal ID prefixes are unsupported")
    paths = sorted(catalog["m_InternalIds"])
    matches = locate_catalog_assets(paths, assets, resources)
    bundles = [name.rsplit("}", 1)[-1].lstrip("/\\").replace("\\", "/")
               for name in paths if name.endswith(".bundle")]
    return {
        "sha256": sha256,
        "matchesWindowsPlayer": sha256 == digest(built),
        "internalIds": paths,
        "keys": keys,
        "locations": locations,
        "assetCandidates": matches,
        "missingBundles": [name for name in bundles if not (built.parent / name).is_file()],
    }


def build_report(recovery):
    project = recovery / "UnityProject/ExportedProject"
    report = static_inventory(project)
    catalog = catalog_inventory(project, recovery, report["assets"], report["resources"])
    additional_roots = {path for match in catalog["assetCandidates"].values() for path in match["paths"]}
    for paths in report["resources"].values():
        additional_roots.update(paths)
    closure, refs, unresolved = walk_references(
        project, sorted(additional_roots),
        group_paths_by_guid(report["assets"])
    )
    report["additionalClosure"] = closure
    report["additionalReferences"] = refs
    report["additionalUnresolvedGuids"] = unresolved
    report["catalog"] = catalog
    report["windowsBuild"] = windows_identity(recovery)
    report["unityVersion"] = text(project / "ProjectSettings/ProjectVersion.txt").splitlines()[0].split(": ", 1)[1]
    report["formatVersion"] = 1
    return report


def group_paths_by_guid(assets):
    by_guid = defaultdict(list)
    for path, guid in assets.items():
        by_guid[guid].append(path)
    return by_guid


def summary(report):
    paths = report["assets"]
    return {
        "unityVersion": report["unityVersion"],
        "scenes": [Path(scene["path"]).stem for scene in report["scenes"]],
        "assetFiles": len(paths),
        "assetExtensions": Counter(Path(name).suffix.lower() for name in paths),
        "sceneClosure": len(report["sceneClosure"]),
        "additionalClosure": len(report["additionalClosure"]),
        "uniqueStaticPaths": len(set(report["sceneClosure"]) | set(report["additionalClosure"])),
        "resourcesKeys": len(report["resources"]),
        "unresolvedSceneGuids": len(report["unresolvedGuids"]),
        "unresolvedAdditionalGuids": len(report["additionalUnresolvedGuids"]),
        "duplicateGuids": len(report["duplicateGuids"]),
        "catalogInternalIds": len(report["catalog"]["internalIds"]),
        "catalogMatchMethods": Counter(row["basis"] for row in report["catalog"]["assetCandidates"].values()),
        "catalogKeys": len(report["catalog"]["keys"]),
        "catalogLocations": len(report["catalog"]["locations"]),
        "catalogMatchesWindowsPlayer": report["catalog"]["matchesWindowsPlayer"],
        "missingBundles": len(report["catalog"]["missingBundles"]),
        "windowsFiles": len(report["windowsBuild"]),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--recovery", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True,
                        help="New JSON file in the local managed run directory")
    args = parser.parse_args()
    report = build_report(args.recovery)
    with args.output.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(report, stream, indent=2, ensure_ascii=False, sort_keys=True)
        stream.write("\n")
    print(json.dumps(summary(report), indent=2, ensure_ascii=True))
    if not report["catalog"]["matchesWindowsPlayer"] or report["catalog"]["missingBundles"]:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
