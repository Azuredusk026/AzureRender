"""Resolve original timer voice scenarios and animation cues."""

import argparse
import json
import math
from pathlib import Path
import re

import yaml

from tools.npr_room.build_voice_room import voice_source_index, voiced_audio
from tools.npr_room.build_work_calendar import PARAMETER_ASSET
from tools.npr_room.reference_inventory import digest
from tools.npr_room.skinned_mesh_export import UnityScene


SCENARIO_COUNTS = {
    "4": 15, "5": 15, "6": 20, "1000": 10, "1001": 10, "1100": 10, "1101": 6,
    "1200": 7, "1201": 10, "1202": 10, "3000": 15, "3001": 8, "3100": 13,
    "3101": 7, "3102": 6, "3200": 15, "3201": 6, "3202": 7,
}
SELECTORS = {
    "longWorkStart": ("longWorkStartVoiceData", "judgeLongMinutes"),
    "continuousWorkStart": ("continuousWorkStartVoiceData", "continuousMinutes"),
    "shortWorkedBreakStart": ("shortWorkedBreakStartVoiceData", "_shortWorkMinutes"),
    "longWorkedBreakStart": ("longWorkedBreakStartVoiceData", "judgeLongMinutes"),
    "longWorkFinish": ("longWorkFinishVoiceData", "judgeLongMinutes"),
    "shortWorkFinish": ("shortWorkFinishVoiceData", "judgeShortMinutes"),
    "midwayFinish": ("midwayFinishVoiceData", None),
}
ANIMATION_ENUM = "Assets/Scripts/Assembly-CSharp/Bulbul/HeroineService.cs"


def timer_voice_parameters(source):
    path = source / PARAMETER_ASSET
    data = UnityScene(path).read(11400000)["PomodoroVoiceData"]
    parameters = {}
    for name, (kind, threshold) in SELECTORS.items():
        row = data[kind]
        probability = row["selectionProbability"]
        if not math.isfinite(probability) or not 0 <= probability <= 100:
            raise ValueError(f"Invalid timer voice selection probability: {name}")
        parameters[name] = {"percent": probability}
        if threshold is not None:
            seconds = row[threshold] * 60
            if not math.isfinite(seconds) or seconds <= 0:
                raise ValueError(f"Invalid timer voice threshold: {name}")
            parameters[name]["seconds"] = seconds
    return parameters, digest(path)


def timer_voice_rows(story, audio, native, counts=SCENARIO_COUNTS):
    sources = voice_source_index(audio)
    translations = {row["ID"]: row for row in story["localization"]}
    media = {}
    categories = {scenario: [] for scenario in counts}
    for group in story["groups"]:
        scenario = group["Scenario"]
        if scenario not in categories:
            continue
        identity = group["ID"]
        index, = story["commandsByGroup"][identity]
        row = story["commands"][index]
        if row["Command"] != "22":
            raise ValueError(f"Expected a timer voice command in {identity}")
        cue = int(row["Arg1"])
        mode = float(row["Arg2"])
        mouth = int(row["Arg3"])
        look = {"scale": float(row["LookScale"]), "seconds": float(row["LookSpeedSeconds"]),
                "ease": int(row["LookEaseType"])}
        if (cue not in (0, 1, 2) or not math.isfinite(mode) or mode < -1 or mouth not in (-1, 0)
                or not all(math.isfinite(value) and value >= 0 for value in look.values())):
            raise ValueError(f"Invalid source timer voice animation instructions: {identity}")
        line, asset = voiced_audio(translations[row["LocalizationID"]], sources, native, identity)
        categories[scenario].append({"group": identity, "episode": int(group["EpisodeNumber"]), "row": index,
                                     **line, "animationCue": cue, "sourceArg2": mode, "speakBeforeMotion": mode == -1,
                                     "moveMouth": mouth != -1, "look": look, "bodyMotion": int(row["BodyMotion"]),
                                     "faceMotion": int(row["FacialMotion"])})
        media.setdefault(asset["guid"], asset)
    for scenario, rows in categories.items():
        if [row["episode"] for row in rows] != list(range(1, counts[scenario] + 1)):
            raise ValueError(f"Invalid original timer voice episode sequence: {scenario}")
    return categories, list(media.values())


def clip_voice_events(path):
    text = path.read_text(encoding="utf-8-sig")
    settings = re.search(r"(?m)^  m_AnimationClipSettings:\n((?:^    [^\n]*\n)+)", text)
    if settings is None or "\n  m_Events:" not in text:
        raise ValueError(f"Animation clip timing is missing: {path.name}")
    clock = yaml.safe_load(settings[1])
    events = yaml.safe_load("m_Events:" + text.rsplit("\n  m_Events:", 1)[1])["m_Events"]
    start, stop = clock["m_StartTime"], clock["m_StopTime"]
    if not math.isfinite(start) or not math.isfinite(stop) or start < 0 or stop <= start:
        raise ValueError(f"Invalid clip time range: {path.name}")
    voice = []
    for event in events:
        if event["functionName"] != "PlayPomodoroVoice":
            continue
        time = event["time"]
        if not math.isfinite(time) or not start <= time <= stop:
            raise ValueError(f"Timer voice event falls outside its clip: {path.name}")
        voice.append({"seconds": time - start, "cue": event["intParameter"]})
    return {"start": start, "stop": stop, "loop": bool(clock["m_LoopTime"]), "voiceEvents": voice}


def timer_voice_motions(source, animator, groups):
    definition = (source / ANIMATION_ENUM).read_text(encoding="utf-8-sig")
    block = definition.split("public enum AnimationType", 1)[1].split("}", 1)[0]
    names = {int(value): name for name, value in re.findall(r"(\w+)\s*=\s*(-?\d+)", block)}
    needed = {}
    for bank in groups.values():
        for row in bank:
            if row["bodyMotion"] >= 0:
                cues = needed.setdefault(row["bodyMotion"], set())
                if not row["speakBeforeMotion"]:
                    cues.add(row["animationCue"])
    motions = {}
    states = animator["states"]
    for motion, cues in sorted(needed.items()):
        name = names[motion]
        state_id, = [key for key, item in states.items() if item["m_Name"] == name]
        route, seen = [], set()
        while state_id:
            if state_id in seen:
                raise ValueError(f"Timer voice motion has a cyclic local route: {name}")
            seen.add(state_id)
            item = states[state_id]
            pointer = item["m_Motion"]
            clip, = animator["externalAssets"][pointer["guid"]]
            path = (source / clip).resolve(strict=True)
            if pointer["fileID"] != 7400000 or path.suffix != ".anim" or not path.is_relative_to(source.resolve()):
                raise ValueError(f"Timer voice motion requires an original animation clip: {clip}")
            transitions = [animator["stateTransitions"][str(edge["fileID"])] for edge in item["m_Transitions"]]
            route.append({"state": state_id, "name": item["m_Name"], "clip": clip, "guid": pointer["guid"],
                          "sha256": digest(path), "speed": item["m_Speed"],
                          **clip_voice_events(path), "outgoing": transitions})
            local = [str(edge["m_DstState"]["fileID"]) for edge in transitions
                     if states.get(str(edge["m_DstState"]["fileID"]), {}).get("m_Name") in (name + "_Loop", name + "_End")]
            if len(local) > 1:
                raise ValueError(f"Multiple local continuations for timer voice motion: {name}")
            state_id = local[0] if local else None
        available = {event["cue"] for clip in route for event in clip["voiceEvents"]}
        if not cues <= available:
            raise ValueError(f"Missing animation marker for timer voice motion {name}: {sorted(cues - available)}")
        motions[str(motion)] = {"requiredCues": sorted(cues), "clips": route}
    return motions


def catalog(source, inventory, audio_catalog, audio_probe, animator):
    settings, parameter_hash = timer_voice_parameters(source)
    voices, media = timer_voice_rows(json.loads(inventory.read_bytes()), json.loads(audio_catalog.read_bytes())["slots"],
                                     json.loads(audio_probe.read_bytes())["files"])
    motions = timer_voice_motions(source, json.loads(animator.read_bytes()), voices)
    return {"schemaVersion": 1, "storySHA256": digest(inventory), "parameterSHA256": parameter_hash,
            "audioCatalogSHA256": digest(audio_catalog), "nativeAudioSHA256": digest(audio_probe),
            "animatorSHA256": digest(animator), "animationEnumSHA256": digest(source / ANIMATION_ENUM),
            "selectors": settings, "groups": voices, "media": media, "motions": motions}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source", "inventory", "audio-catalog", "audio-probe", "animator", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    data = catalog(args.source, args.inventory, args.audio_catalog, args.audio_probe, args.animator)
    with args.output.open("x", encoding="utf-8", newline="\n") as target:
        json.dump(data, target, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
        target.write("\n")
    print(json.dumps({"groups": len(data["groups"]), "lines": sum(map(len, data["groups"].values())),
                      "audioFiles": len(data["media"]), "motions": len(data["motions"])}))
