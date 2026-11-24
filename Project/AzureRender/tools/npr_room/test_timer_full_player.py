"""Replay both timer modes with the production voice bank and original media."""

import argparse
import json
from pathlib import Path

from reference_inventory import digest
from test_timer_voice_player import opened_voice, run_room


def describes_voice(row, voice, cue, state, window, motions):
    clip, time, _, frame = voice
    if row["bodyMotion"] < 0:
        return clip == state and frame == cue[3]
    motion = motions[str(row["bodyMotion"])]
    if row["speakBeforeMotion"]:
        return clip == state and frame == cue[3] and any(
            sample[0] == motion["clipIndex"] and sample[3] > frame for sample in window)
    delta = time - motion["cues"][str(row["animationCue"])]
    return clip == motion["clipIndex"] and -1e-6 <= delta <= 1 / 60 + 1e-6


def replay(player, project, resources, output, catalog_path, mode):
    receipt = json.loads(project.with_name("timer-voice-receipt.json").read_bytes())
    assert not receipt["cueReplay"] and receipt["catalogSHA256"] == digest(catalog_path)
    catalog = json.loads(catalog_path.read_bytes())
    assert set(receipt["motions"]) == set(catalog["motions"]) and len(receipt["motions"]) == 16
    assert receipt["availableAudioSources"] == len(catalog["media"]) == 113
    media_hashes = {source["asset"]: source["sha256"] for source in catalog["media"]}
    steps = [(30, 84, "work", ("4",), 1), (750, 75, "break", ("5", "1100"), 0),
             (1470, 82, "finish", ("6", "1201", "1202"), 0)]
    options = []
    if mode == "countup":
        options = [(10, 77)]
        steps = [(30, 84, "work", ("3000",), 1), (750, 84, "break", ("3100", "3101"), 0),
                 (1470, 84, "resume", ("3000", "3001"), 1), (2190, 82, "finish", ("3200", "3201"), 0)]
    keys = options + [(frame, key) for frame, key, *_ in steps]
    report, runtime, timeline = run_room(player, project, resources, output, keys, steps[-1][0] + 720)
    files = opened_voice(output)
    assert len(files) == len(steps) and report["audioStarts"] == 1 + len(options) + 2 * len(steps), files
    audio_frames = [sample for sample in timeline if sample[2]]
    first_action = next(index for index, sample in enumerate(audio_frames) if sample[3] >= steps[0][0])
    voice_frames = [sample for sample in audio_frames[first_action:] for _ in range(sample[2])]
    assert len(voice_frames) == 2 * len(steps), voice_frames
    observed = []
    for index, (name, (_, _, event, groups, state)) in enumerate(zip(files, steps, strict=True)):
        cue, voice = voice_frames[2 * index:2 * index + 2]
        boundary = voice_frames[2 * index + 2][3] if index + 1 < len(steps) else timeline[-1][3] + 1
        window = [sample for sample in timeline if cue[3] <= sample[3] < boundary]
        matches = [row for group in groups for row in catalog["groups"][group]
                   if Path(row["asset"]).name == name and describes_voice(row, voice, cue, state, window, receipt["motions"])]
        assert matches, (event, name, groups, cue, voice)
        asset = matches[0]["asset"]
        assert digest(project.parent / "assets" / asset) == media_hashes[asset]
        observed.append({"event": event, "groups": sorted({row["group"] for row in matches}), "file": name,
                         "cueFrame": cue[3], "voiceFrame": voice[3], "clip": voice[0], "clipTime": voice[1]})
    assert timeline[-1][0] == 0, timeline[-1]
    proof = {"mode": mode, "audioStarts": report["audioStarts"], "observed": observed, "frames": len(timeline),
             "runtimeSHA256": digest(runtime), "playerSHA256": digest(player)}
    (output / "timer-full-replay.json").write_text(json.dumps(proof), encoding="utf-8")
    return proof


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("player", "project", "resource-root", "output", "catalog"):
        parser.add_argument("--" + argument, type=Path, required=True)
    parser.add_argument("--mode", choices=("pomodoro", "countup"), required=True)
    args = parser.parse_args()
    print(json.dumps(replay(args.player.resolve(), args.project.resolve(), args.resource_root.resolve(),
                            args.output.resolve(), args.catalog.resolve(), args.mode)))
