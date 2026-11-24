"""Verify operable music controls after unavailable original audio and explicit retry."""
import argparse
import json
import os
from pathlib import Path

from test_notes_player import playing
from test_original_music_player import digest, fixture


def verify(player, resources, source, output):
    os.environ["VK_INSTANCE_LAYERS"] = "VK_LAYER_KHRONOS_validation"
    os.environ["VK_LOADER_DEBUG"] = "layer"
    output.mkdir()
    project = fixture(output / "fixture", source)
    tone = project.parent / "assets/tone-1.wav"
    withheld = tone.with_suffix(".unavailable")
    withheld.write_bytes(tone.read_bytes())
    tone.write_bytes(tone.read_bytes()[:16])
    control = {}
    profile = output / "profile"
    expected = "Truncated WAV container"
    try:
        with playing(player, project, resources, profile, output / "missing",
                     on_ready=lambda _, call: control.update(call=call),
                     expected_presentation_errors=(expected,)) as (click, _, _, screenshot):
            click("music-toggle")
            state = json.loads(control["call"]("query", name="game.state"))
            assert state["audioStarts"] == 0 and expected in state["presentationErrors"], state
            screenshot("unavailable.png")
            click("music-quiet")
            slot, = profile.glob("project-*/save-music.json")
            assert json.loads(slot.read_bytes())["payload"] == [1, False, 45, False, False, False]
            tone.write_bytes(withheld.read_bytes())
            withheld.unlink()
            click("music-play")
            control["call"]("wait-frames", frames=12)
            state = json.loads(control["call"]("query", name="game.state"))
            assert state["audioStarts"] >= 1 and not state["scriptErrors"], state
            assert json.loads(slot.read_bytes())["payload"] == [1, True, 45, False, False, False]
            screenshot("recovered.png")
    finally:
        if withheld.exists():
            tone.write_bytes(withheld.read_bytes())
            withheld.unlink()
    with playing(player, project, resources, profile, output / "reopen") as (click, _, _, _):
        click("music-toggle")
        click("music-play")
        assert json.loads(slot.read_bytes())["payload"] == [1, False, 45, False, False, False]
    for name in ("missing", "reopen"):
        text = (output / name / "stderr.log").read_text(encoding="utf-8")
        assert 'Insert instance layer "VK_LAYER_KHRONOS_validation"' in text
        assert "VUID-" not in text and "SYNC-HAZARD" not in text
    result = {"playerSHA256": digest(player), "controllerSHA256": digest(source),
              "originalToneSHA256": digest(tone), "expectedDiagnostic": expected,
              "recoveredAudioStarts": state["audioStarts"], "independentPlayers": 2,
              "scriptErrors": state["scriptErrors"], "liveAllocationsAfterExit": 0,
              "preferences": json.loads(slot.read_bytes())["payload"]}
    (output / "verification.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("player", "resources", "source", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    verify(args.player, args.resources, args.source, args.output)
