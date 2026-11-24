"""Verify audio payload versions and recovery through native production controls."""
import argparse
import json
import os
from pathlib import Path
from test_notes_player import playing
from test_original_music_player import fixture as music_fixture, digest, write
from test_original_ambient_player import fixture as ambient_fixture
from test_unlock_room_player import save


def verify(player, resources, source, output):
    os.environ["VK_INSTANCE_LAYERS"] = "VK_LAYER_KHRONOS_validation"
    os.environ["VK_LOADER_DEBUG"] = "layer"
    output.mkdir()
    results = {}
    for kind, factory in (("music", music_fixture), ("ambient", ambient_fixture)):
        root = output / kind
        root.mkdir()
        controller = source / f"{kind}.lua"
        project = factory(root / "fixture", controller)
        profile = root / "profile"
        save(profile, project, kind, [2, ["future preferences"]])
        slot, = profile.glob(f"project-*/save-{kind}.json")
        protected = slot.read_bytes()
        with playing(player, project, resources, profile, root / "protect") as (click, _, _, screenshot):
            click(kind + "-toggle")
            click(kind + ("-quiet" if kind == "music" else "-loud"))
            assert slot.read_bytes() == protected, kind
            screenshot("future-protected.png")
            compatible = [1, False, 30, False, False] if kind == "music" else [1, [[1, 20, False]]]
            save(profile, project, kind, compatible)
            click(kind + "-loud")
            expected = [1, True, 50, False, False, False] if kind == "music" else [1, [[1, 10, False]]]
            assert json.loads(slot.read_bytes())["payload"] == expected, kind
            stored = slot.read_bytes()
            external = b'{ "schemaVersion": 1, "payload": [3, ["external future value"]] }'
            slot.write_bytes(external)
            click(kind + ("-mute" if kind == "music" else "-loud"))
            assert slot.read_bytes() == external, kind
            slot.write_bytes(stored)
            click(kind + "-quiet")
            expected = [1, True, 45, True, False, False] if kind == "music" else [1, [[1, 10, False]]]
            assert json.loads(slot.read_bytes())["payload"] == expected, kind
            screenshot("compatible-restored.png")
        with playing(player, project, resources, profile, root / "reopen") as (click, _, _, screenshot):
            click(kind + "-toggle")
            click(kind + "-mute")
            reopened = [1, True, 45, False, False, False] if kind == "music" else [1, [[1, 10, True]]]
            assert json.loads(slot.read_bytes())["payload"] == reopened, kind
            screenshot("reopened.png")
        for stage in ("protect", "reopen"):
            text = (root / stage / "stderr.log").read_text(encoding="utf-8")
            assert 'Insert instance layer "VK_LAYER_KHRONOS_validation"' in text
            assert "VUID-" not in text and "SYNC-HAZARD" not in text
        results[kind] = {"controllerSHA256": digest(controller), "initialPayloadVersion": 2,
                         "externalPayloadVersion": 3, "protectedOriginalBytes": True,
                         "sameSessionRecovery": expected, "reopened": reopened}
    results.update(playerSHA256=digest(player), independentPlayers=4, liveAllocationsAfterExit=0)
    write(output / "verification.json", results)
    print(json.dumps(results))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("player", "resources", "source", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    verify(args.player, args.resources, args.source, args.output)
