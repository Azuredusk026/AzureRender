"""Audit the Recovery startup chain and corresponding Player project bindings."""

import argparse
import json
from pathlib import Path
import re

from tools.npr_room.reference_inventory import digest, group_paths_by_guid, text
from tools.npr_room.skinned_mesh_export import UnityScene


ORIGINAL_SCRIPTS = ("BootBehavior", "EntryBehavior", "SceneLoadHandle", "SceneLoader")
ORIGINAL_SCENES = ("Boot", "AssetLoad", "Entry", "Transition", "RoomScene")
PLAYER_SCENES = ("boot", "asset-load", "entry", "transition", "room")
STARTUP_NODES = ("startup", "asset-load", "entry", "transition")
LEVEL = r'"(assets:/[\w-]+\.azurelevel)"'


def unique_match(source, pattern, label):
    matches = list(re.finditer(pattern, source))
    if len(matches) != 1:
        raise ValueError(f"Expected one {label}")
    return matches[0]


def recovery_route(boot, entry, handle, loader):
    initialize = unique_match(boot, r"await Addressables\.InitializeAsync\(\)", "catalog initialization")
    additive = list(re.finditer(r'await SceneLoader\.LoadSceneAdditiveAsync\("(\w+)"\)', boot))
    if [match[1] for match in additive] != ["AssetLoad", "Entry"] or initialize.end() > additive[0].start():
        raise ValueError("Unexpected original boot route")
    unique_match(loader, r"SceneManager\.LoadSceneAsync\(sceneName, LoadSceneMode\.Additive\)", "additive overload")
    unique_match(loader, r"SceneLoadHandle\.Load\(sceneName, autoActivate\)", "scene handle wrapper")
    target = unique_match(entry, r'SceneLoader\.LoadSceneAsync\("(\w+)", autoActivate:\s*false\)',
                          "deferred room load")
    transition = unique_match(handle, r'await SceneManager\.LoadSceneAsync\("(\w+)"\)', "transition scene load")
    prepare = unique_match(handle, r"SceneManager\.LoadSceneAsync\(sceneName\)", "target scene load")
    auto = unique_match(handle, r"_mainOperation\.allowSceneActivation = autoActivate", "deferred activation assignment")
    readiness = unique_match(handle, r"_mainOperation\.progress\s*>=\s*(\d+\.\d+)f", "readiness threshold")
    unique_match(handle, r"_mainOperation\.allowSceneActivation = true", "manual activation assignment")
    times = (
        unique_match(entry, r"float startTime = Time\.time;\s*await LoadAsync\(", "entry clock"),
        target,
        unique_match(entry, r"await UniTask\.WaitUntil\(\(\) => \w+\.IsReady\)", "scene readiness wait"),
        unique_match(entry, r"Mathf\.Max\(0f,\s*(\d+(?:\.\d+)?)f\s*-\s*\w+\)", "entry minimum delay"),
        unique_match(entry, r"await UniTask\.Delay\(TimeSpan\.FromSeconds\((\d+\.\d+)\)\)", "reveal delay"),
        unique_match(entry, r"await UniTask\.WaitUntil\(\(\) => loadDirectionService\.IsFinishDirection\(",
                     "reveal completion wait"),
        unique_match(entry, r"\b\w+\.Activate\(\)", "room activation"),
    )
    scenes = ["Boot", *(match[1] for match in additive), transition[1], target[1]]
    if (scenes != list(ORIGINAL_SCENES) or not transition.end() < prepare.start() < auto.start()
            or any(before.end() > after.start() for before, after in zip(times, times[1:]))):
        raise ValueError("Unexpected original scene activation order")
    return {"steps": [{"scene": scene, "mode": mode} for scene, mode in
                      zip(scenes, ("startup", "additive", "additive", "single", "manual-activation"))],
            "readyProgress": float(readiness[1]), "minimumSeconds": float(times[3][1]),
            "minimumClock": "entry-before-LoadAsync", "delayAfterRevealSeconds": float(times[4][1]),
            "waitsForRevealCompletion": True}


def candidate_route(startup, script):
    targets = [f"assets:/{name}.azurelevel" for name in PLAYER_SCENES]
    room = unique_match(script, r"\blocal room = " + LEVEL, "room route target")[1]
    navigation = unique_match(script, r"\blocal next_level = \{([^}]+)\}", "room route map")[1]
    edges = [(key or bracketed, target) for key, bracketed, target in
             re.findall(r'(?:\b([\w-]+)|\["([\w-]+)"\])\s*=\s*' + LEVEL, navigation)]
    if (startup != targets[0] or room != targets[-1]
            or dict(edges) != dict(zip(("startup", "asset-load", "entry"), targets[1:4])) or len(edges) != 3):
        raise ValueError("Unexpected candidate startup route")
    prepare = unique_match(script, r"self:preload_level\(room\)", "candidate room preload")
    activate = unique_match(script, r"self:load_level\(room\)", "candidate room activation")
    minimum = unique_match(script, r"loading_seconds < (\d+(?:\.\d+)?) or not self:level_ready\(room\)",
                           "candidate readiness wait")
    delay = unique_match(script, r"reveal_seconds >= (\d+(?:\.\d+)?)", "candidate reveal delay")
    if not prepare.end() < minimum.start() < delay.start() < activate.start():
        raise ValueError("Unexpected candidate scene activation order")
    return {"levels": targets, "minimumTransitionSeconds": float(minimum[1]),
            "minimumClock": "transition-local-clock", "readyDelaySeconds": float(delay[1])}


def candidate_bindings(project):
    base = project.parent
    config = json.loads(project.read_bytes())
    mount, = [Path(entry["path"]) for entry in config["mounts"] if entry["name"] == "assets"]
    root = (base / mount).resolve()
    if mount.is_absolute() or not root.is_relative_to(base.resolve()):
        raise ValueError("Expected the candidate assets mount beneath its project")
    flow = root / "startup-flow.lua"
    route = candidate_route(config["startupScene"], text(flow))
    bindings = []
    for index, (reference, expected) in enumerate(zip(route["levels"], PLAYER_SCENES)):
        path = root / reference.removeprefix("assets:/")
        scene = json.loads(path.read_bytes())
        if scene["id"] != expected:
            raise ValueError(f"Candidate level identity differs: {path.name}")
        attached = {}
        startup_nodes = []
        for node in scene["nodes"]:
            for kind, component in node.get("components", {}).items():
                if kind not in ("azure.script", "azure.game-ui"):
                    continue
                asset = component["data"]["asset"]
                resolved = (root / asset.removeprefix("assets:/")).resolve()
                if not asset.startswith("assets:/") or not resolved.is_relative_to(root) or not resolved.is_file():
                    raise ValueError(f"Missing mounted {kind} target in {path.name}: {asset}")
                attached[asset] = digest(resolved)
                if kind == "azure.script" and asset == "assets:/startup-flow.lua":
                    startup_nodes.append(node["id"])
        if index < len(STARTUP_NODES) and startup_nodes != [STARTUP_NODES[index]]:
            raise ValueError(f"Candidate startup controller binding differs in {path.name}")
        if expected == "room" and not {"assets:/pomodoro.lua", "assets:/timer.rml"} <= attached.keys():
            raise ValueError("Candidate room timer or interface binding is missing")
        bindings.append({"reference": reference, "sha256": digest(path), "attached": attached})
    return route | {"projectSHA256": digest(project), "bindings": bindings}


def scene_owner(scene, script_guid, fields, assets_by_guid):
    owners = []
    for file_id, (kind, start, end) in scene.documents.items():
        if kind != 114 or script_guid not in scene.serialized[start:end]:
            continue
        record = scene.read(file_id)
        if record["m_Script"].get("guid") == script_guid:
            owners.append((file_id, record))
    if len(owners) != 1:
        raise ValueError(f"Expected one scene owner for {script_guid}")
    file_id, record = owners[0]
    object_id = record["m_GameObject"]["fileID"]
    owner = scene.read(object_id)
    refs = {}
    for field in fields:
        reference = record[field]
        paths = assets_by_guid.get(reference.get("guid"), [])
        if len(paths) != 1 or not reference["fileID"]:
            raise ValueError(f"Unresolved startup reference: {field}")
        refs[field] = {"guid": reference["guid"], "fileId": reference["fileID"], "assetPath": paths[0]}
    return {"componentFileId": file_id, "componentEnabled": bool(record["m_Enabled"]), "scriptGuid": script_guid,
            "objectFileId": object_id, "objectName": owner["m_Name"], "objectActive": bool(owner["m_IsActive"]),
            "directReferences": refs}


def startup_dependencies(source, scenes, inventory):
    assets = inventory["assets"]
    by_guid = group_paths_by_guid(assets)
    result = {}
    for name, component, fields in (
        ("AssetLoad", "AssetLoadHolder", ("projectLifetimeScopePrefab", "vContainerSettings", "audioManagerSetting")),
        ("RoomScene", "RoomLifetimeScope", ()),
    ):
        path = source / scenes[name]["path"]
        script = f"Assets/Scripts/Assembly-CSharp/Bulbul/{component}.cs"
        binding = scene_owner(UnityScene(path), assets[script], fields, by_guid)
        result[name] = {"sceneSHA256": digest(path), "component": component, **binding}
    return result


def build_report(source, inventory, project):
    folder = source / "Assets/Scripts/Assembly-CSharp/Bulbul"
    scripts = [folder / (name + ".cs") for name in ORIGINAL_SCRIPTS]
    original = recovery_route(*(text(path) for path in scripts))
    reference = json.loads(inventory.read_bytes())
    scenes = {Path(row["path"]).stem: row for row in reference["scenes"]}
    if set(scenes) != set(ORIGINAL_SCENES):
        raise ValueError("The source lock must cover the five referenced scenes")
    original["sceneInventory"] = [scenes[name] for name in ORIGINAL_SCENES]
    original["scripts"] = {path.relative_to(source).as_posix(): digest(path) for path in scripts}
    original["startupDependencies"] = startup_dependencies(source, scenes, reference)
    return {"schemaVersion": 1, "sourceInventorySHA256": digest(inventory),
            "recovery": original, "candidate": candidate_bindings(project)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source", "inventory", "project", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    report = build_report(args.source, args.inventory, args.project)
    with args.output.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(report, stream, indent=2, sort_keys=True)
        stream.write("\n")


if __name__ == "__main__":
    main()
