"""Connect the first chapter's source controller conditions to the normal room."""

import argparse
from collections import defaultdict
import json
import math
from pathlib import Path
import shutil
import tempfile

from tools.npr_room.build_story_book_room import skip_capture_orphans
from tools.npr_room.build_story_motion_room import GRAPH, EXAMPLE
from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest
from tools.npr_room.room_geometry import copy_room_asset
from tools.npr_room.story_body_probe import CAMERA_START, FIRST_CHAPTER, TUTORIAL
from tools.npr_room.story_chapters import lua_value


GROUP = "main_01"
CONDITIONS = {3: int.__gt__, 4: int.__lt__, 6: int.__eq__, 7: int.__ne__}
TRIGGERS = {"Story_SubBase001_Fun": 1003, "GameStart_End": 401}


def load(path):
    return json.loads(path.read_bytes())


def enabled(edge, number, parameters):
    for condition in edge["m_Conditions"]:
        field = condition["m_ConditionEvent"]
        mode = condition["m_ConditionMode"]
        if field == "MotionType" and mode in CONDITIONS:
            value = condition["m_EventTreshold"]
            if value != int(value):
                raise ValueError(f"Non-integral original MotionType threshold: {value}")
            if not CONDITIONS[mode](number, int(value)):
                return False
        elif parameters[field]["m_Type"] == 9 and mode == 1:
            if number != TRIGGERS.get(field):
                return False
        elif parameters[field]["m_Type"] == 4 and mode in (1, 2):
            if bool(parameters[field]["m_DefaultBool"]) != (mode == 1):
                return False
        else:
            raise ValueError(f"Unprojected opening controller condition {field=} {mode=}")
    return True


def compile_signals(body, controller, catalog, durations, camera=None):
    if body["schemaVersion"] not in (3, 4):
        raise ValueError("Expected the assembled timed body graph")
    chapter = "tutorial" if camera else "main"
    commands = TUTORIAL if camera else FIRST_CHAPTER
    root_layer = controller["layers"][0]
    if root_layer["m_Name"] != "Base Layer":
        raise ValueError("Missing original base layer")
    root = str(root_layer["m_StateMachine"]["fileID"])
    machines = controller["machines"]
    authored_states = controller["states"]
    entry_edges = controller["entryAndMachineTransitions"]
    parameters = {parameter["m_Name"]: parameter for parameter in controller["parameters"]}
    signal = {number: chapter + "-motion-" + number for number in commands}
    state_by_name = {state["name"]: state["clip"] for state in body["states"]}
    selected = {}
    for number in commands:
        for row in catalog["motions"][number]["states"]:
            for variant in row["variants"]:
                key = variant["id"]
                if selected.setdefault(key, row) != row:
                    raise ValueError(f"Conflicting source body bindings: {key}")
    if camera:
        for key in CAMERA_START:
            state = authored_states[key]
            clip, = controller["externalAssets"][state["m_Motion"]["guid"]]
            if clip not in camera:
                raise ValueError("Expected the sampled tutorial camera asset")
            selected[key] = {"state": state["m_Name"], "clip": clip}
    owner = {str(child["m_State"]["fileID"]): key for key, machine in machines.items()
             for child in machine["m_ChildStates"]}
    parent = {str(child["m_StateMachine"]["fileID"]): key for key, machine in machines.items()
              for child in machine["m_ChildStateMachines"]}

    def path(key):
        result = []
        while key in machines:
            result.append(machines[key]["m_Name"])
            if key == root:
                return ".".join(reversed(result))
            key = parent[key]
        raise ValueError("Opening states must belong to the base layer")

    source_paths = {key: path(owner[key]) + "." + row["state"] for key, row in selected.items()}
    aliases = {key: chapter + "-" + name for key, name in source_paths.items()}

    def check_edge(edge):
        if edge["m_Solo"] or edge["m_Mute"]:
            raise ValueError("Opening state requires filtered original transitions")

    def enter(key, number):
        machine = machines[key]
        if machine["m_StateMachineBehaviours"]:
            raise ValueError("Opening machine requires a script behaviour")
        for link in machine["m_EntryTransitions"]:
            edge = entry_edges[str(link["fileID"])]
            if enabled(edge, number, parameters):
                check_edge(edge)
                return dest(key, edge, number)
        return str(machine["m_DefaultState"]["fileID"])

    def dest(machine, edge, number):
        if edge["m_IsExit"]:
            if machine != root:
                outer = machines[parent[machine]]["m_StateMachineTransitions"]
                if any(str(item["first"]["fileID"]) == machine for item in outer):
                    raise ValueError("An opening machine exit requires its parent transition table")
                return str(machines[root]["m_DefaultState"]["fileID"])
            return enter(root, number)
        if edge["m_DstStateMachine"]["fileID"]:
            return enter(str(edge["m_DstStateMachine"]["fileID"]), number)
        return str(edge["m_DstState"]["fileID"])

    entries = {number: enter(root, int(number)) for number in signal}
    for link in machines[root]["m_AnyStateTransitions"]:
        edge = controller["stateTransitions"][str(link["fileID"])]
        if any(enabled(edge, int(number), parameters) for number in signal):
            raise ValueError("An opening command requires a base layer Any State route")
    if not set(entries.values()) <= aliases.keys():
        raise ValueError("Opening command requires an unsampled entry state")
    clips, seconds, branches = {}, {}, []
    repeated = None
    timeline = list(body["timeline"])
    for key, row in selected.items():
        state = authored_states[key]
        if state["m_Name"] != row["state"] or state["m_StateMachineBehaviours"]:
            raise ValueError(f"Opening source state has an unprojected behaviour or name: {key}")
        binding = durations[Path(row["clip"]).stem]
        clips[key] = state_by_name["story-" + row["state"]]
        seconds[key] = binding["seconds"]
        if clips[key] != binding["clipIndex"] or not math.isfinite(seconds[key]) or seconds[key] <= 0:
            raise ValueError(f"Different sampled clip for source state: {key}")
    for key, row in selected.items():
        candidates = [controller["stateTransitions"][str(link["fileID"])]
                      for link in authored_states[key]["m_Transitions"]]
        if camera and key == CAMERA_START[0]:
            edge, = candidates
            check_edge(edge)
            target = dest(owner[key], edge, 400)
            if (edge["m_Conditions"] or not edge["m_HasFixedDuration"] or not edge["m_HasExitTime"]
                    or edge["m_InterruptionSource"] or target != CAMERA_START[1]):
                raise ValueError("Expected the original tutorial camera loop entrance")
            timeline.append({"from": aliases[key], "to": aliases[target],
                             "afterSeconds": edge["m_ExitTime"] * seconds[key],
                             "startSeconds": edge["m_TransitionOffset"] * seconds[target],
                             "blendSeconds": edge["m_TransitionDuration"]})
            continue
        grouped = defaultdict(list)
        for number, name in signal.items():
            active = [edge for edge in candidates if enabled(edge, int(number), parameters)]
            if not active:
                continue
            # The camera loop tests the end trigger before its MotionType fallback.
            if len(active) != 1 and not (camera and key == CAMERA_START[1] and number == "401"):
                raise ValueError(f"Source conditions for {key} overlap on opening motion {number}")
            edge = active[0]
            check_edge(edge)
            if edge["m_InterruptionSource"] or (not edge["m_HasFixedDuration"] and key != CAMERA_START[1]):
                raise ValueError("Expected fixed, non-interruptible source transitions")
            target = dest(owner[key], edge, int(number))
            if target not in seconds:
                raise ValueError(f"Opening transition reaches an unprojected state: {key} -> {target}")
            once = any(condition["m_ConditionEvent"] in TRIGGERS for condition in edge["m_Conditions"])
            at = edge["m_ExitTime"] * seconds[key] if edge["m_HasExitTime"] else 0
            offset = edge["m_TransitionOffset"] * seconds[target]
            blend = edge["m_TransitionDuration"] * (1 if edge["m_HasFixedDuration"] else seconds[key])
            if target == key:
                if not (camera and key == entries["0"] and number == "401" and blend > 0
                        and edge["m_CanTransitionToSelf"] and at == 0):
                    raise ValueError(f"Unexpected same-state entrance for {key}")
                repeated = key
                # Unity checks this exit on the evaluation after the blend completes.
                at = blend + 1 / 60
            grouped[(target, at, offset, blend, once)].append(name)
        for (target, at, offset, blend, once), names in grouped.items():
            branches.append({"from": aliases[key], "to": aliases[target], "signals": names, "once": once,
                             "afterSeconds": at, "startSeconds": offset, "blendSeconds": blend})
    additions = [{"name": aliases[key], "clip": clip} for key, clip in clips.items()]
    if repeated is not None:
        name, duplicate = aliases[repeated], aliases[repeated] + "-repeat"
        additions.append({"name": duplicate, "clip": clips[repeated]})
        for row in tuple(branches):
            if row["from"] != name:
                continue
            twin = {**row, "from": duplicate}
            if row["to"] == name:
                row["to"] = duplicate
            branches.append(twin)
    route = {"entry": aliases[entries["0"]], "signals": signal}
    if camera:
        route["camera"] = aliases[CAMERA_START[0]]
    graph = {**body, "schemaVersion": 4, "signals": [*body.get("signals", ()), *signal.values()],
             "branches": [*body.get("branches", ()), *branches], "timeline": timeline,
             "states": [*body["states"], *additions]}
    return graph, route, {source_paths[key]: {"name": aliases[key], "clip": clips[key], "seconds": seconds[key]}
                          for key in aliases}


def integration(script, route, tutorial=None):
    body_anchor, desk_anchor = "function new_story_body(", "function new_story_desk("
    config_anchor = "local story_body_states = "
    selector = "new_story_body(setMotion, story_body_states)"
    if any(script.count(a) != 1 for a in (body_anchor, desk_anchor, config_anchor, selector)):
        raise ValueError("Expected the normal story and desk hooks")
    start, end = script.index(body_anchor), script.index(desk_anchor)
    if end <= start:
        raise ValueError("Expected the body helper before the desk helper")
    body = (EXAMPLE / "story-body.lua").read_text(encoding="utf-8").rstrip()
    routes = {GROUP: route}
    if tutorial:
        routes["tutorial_01"] = tutorial
        effect = '        if books then books:effect(row) end'
        camera = '        if body and body.effect then body:effect(row) end'
        if camera not in script:
            if script.count(effect) != 1:
                raise ValueError("Expected the normal story effect hook")
            script = script.replace(effect, effect + "\n" + camera)
    config = "local story_body_routes = " + lua_value(routes) + "\n"
    return (script[:start] + body + "\n\n" + script[end:]).replace(
        config_anchor, config + config_anchor).replace(
        selector, "new_story_body(setMotion, story_body_states, story_body_routes)")


def inputs(room, catalog_path, graph_path, chapters_path):
    previous = load(room / "opening-story-motion-receipt.json")
    catalog = load(catalog_path)
    if (previous["graphSHA256"] != digest(room / "assets" / GRAPH)
            or previous["catalogSHA256"] != digest(catalog_path)
            or catalog["sourceSHA256"]["graph"] != digest(graph_path)
            or previous["chaptersSHA256"] != digest(chapters_path)):
        raise ValueError("Expected the room's source controller and opening chapters")
    chapter, = (row for row in load(chapters_path)["chapters"] if row["group"] == GROUP)
    commands = [str(row["bodyMotion"]) for row in chapter["commands"]
                if row["command"] in ("Text", "ChangeMotion") and row.get("bodyMotion", -1) != -1]
    if set(commands) != set(FIRST_CHAPTER) or commands[0] != "0" or commands.count("1003") != 1:
        raise ValueError("Opening signal projection requires the locked first chapter commands")
    durations = load(room / "opening-body-receipt.json")["motions"]
    return compile_signals(load(room / "assets" / GRAPH), load(graph_path), catalog, durations)


def stage(room, catalog_path, graph_path, chapters_path, output):
    body, route, bindings = inputs(room, catalog_path, graph_path, chapters_path)
    code = integration((room / "assets/pomodoro.lua").read_text(encoding="utf-8"), route)
    with tempfile.TemporaryDirectory(prefix="story-body-signals-", dir=output.parent) as directory:
        project = Path(directory) / "project"
        assets = project / "assets"
        shutil.copytree(room / "assets", assets, copy_function=copy_room_asset, ignore=skip_capture_orphans)
        for path in [room / "project.azureproject", *room.glob("*-receipt.json")]:
            shutil.copyfile(path, project / path.name)
        write(assets / GRAPH, body)
        script = assets / "pomodoro.lua"
        script.write_text(code, encoding="utf-8", newline="\n")
        for name in ("opening-desk-receipt.json", "opening-story-room-receipt.json", "opening-story-motion-receipt.json"):
            receipt = load(project / name)
            receipt["scriptSHA256"] = digest(script)
            if name == "opening-story-motion-receipt.json":
                receipt.update(graphSHA256=digest(assets / GRAPH), controllerIndexSHA256=digest(graph_path),
                               signals={GROUP: route}, conditionalStates=bindings, branches=body["branches"])
            write(project / name, receipt)
        publish_directory(project, output)
    return receipt


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for arg in ("room", "catalog", "graph", "chapters", "output"):
        parser.add_argument("--" + arg, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(stage(args.room, args.catalog, args.graph, args.chapters, args.output), ensure_ascii=False))
