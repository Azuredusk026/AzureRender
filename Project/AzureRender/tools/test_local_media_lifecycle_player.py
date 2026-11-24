"""Cancel and close the real Player while its media import owns staging work."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import time
import wave

from build_local_media_example import build
from test_local_media_player import native_selection, snapshot_bytes
from npr_room.test_notes_player import playing
from npr_room.test_original_music_player import digest, write


def large_tone(path):
    byte_count = 96 * 1024 * 1024
    block = (struct.pack("<h", 4000) * 24 + struct.pack("<h", -4000) * 24) * 1024
    with wave.open(str(path), "wb") as audio:
        audio.setparams((1, 2, 48000, byte_count // 2, "NONE", "not compressed"))
        remaining = byte_count
        while remaining:
            count = min(remaining, len(block))
            audio.writeframesraw(block[:count]); remaining -= count


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def verify(player, resources, output):
    output.mkdir()
    project = build(output / "workspace")
    source = output / "活动 导入.wav"
    large_tone(source)
    source_hash = sha(source)
    original_bytes = source.stat().st_size
    profile, control = output / "profile", {}
    os.environ["VK_INSTANCE_LAYERS"] = "VK_LAYER_KHRONOS_validation"
    os.environ["VK_LOADER_DEBUG"] = "layer"
    def begin(window, call): control.update(pid=window.pid, call=call)
    def staging():
        return [path for root in profile.glob("project-*/.content-local-media-music-staging")
                for path in root.iterdir()]
    def rows():
        files = list(profile.glob("project-*/save-local-media-music.json"))
        return json.loads(snapshot_bytes(files[0]))["payload"][1] if files else []
    def import_source(click, path):
        native_selection(control["pid"], "导入本地音乐", [path], lambda: click("media-import-music"))
    def settle():
        deadline = time.monotonic() + 10
        while staging():
            assert time.monotonic() < deadline, "owned staging did not retire"
            control["call"]("wait-frames", frames=2)

    observations = []
    with playing(player, project, resources, profile, output / "cancel", on_ready=begin) as (click, _, _, screenshot):
        import_source(click, source)
        active = staging()
        assert active and not rows(), "The cancellation check requires an active unpublished preparation"
        observations.append(dict(case="cancel", staging=[str(p) for p in active], committedBefore=0))
        click("media-cancel")
        settle()
        assert not rows(), "Cancelling preparation must preserve the empty committed library"
        control["call"]("wait-frames", frames=4)
        screenshot("cancelled-import.png")
    assert not staging() and not rows()
    with playing(player, project, resources, profile, output / "close", on_ready=begin) as (click, _, _, _):
        import_source(click, source)
        active = staging()
        assert active and not rows(), "The exit check requires an active unpublished preparation"
        observations.append(dict(case="close", staging=[str(p) for p in active], committedBefore=0))
        # The context sends the normal native window-close message immediately
        # after its final state observation, then requires clean process exit.
    assert not staging(), "Normal Player exit must join its worker and retire staging"
    committed = rows()
    assert len(committed) <= 1
    if committed:
        row, = committed
        slot, = profile.glob("project-*/save-local-media-music.json")
        cached = slot.parent / "content-local-media-music" / row[3] / row[4]
        assert sha(cached) == source_hash and cached.stat().st_size == original_bytes
    assert sha(source) == source_hash and source.stat().st_size == original_bytes
    relocated = output / "源文件已释放.wav"
    assert source.resolve().parent == output.resolve() and relocated.resolve().parent == output.resolve()
    source.rename(relocated)
    assert sha(relocated) == source_hash
    with playing(player, project, resources, profile, output / "reopen", on_ready=begin) as (click, _, _, screenshot):
        import_source(click, project.parent / "assets/placeholder.wav")
        deadline = time.monotonic() + 15
        while not any(row[4] == "placeholder.wav" for row in rows()):
            assert time.monotonic() < deadline
            control["call"]("wait-frames", frames=4)
        control["call"]("wait-frames", frames=5)
        screenshot("reopened-import.png")
    for name in ("cancel", "close", "reopen"):
        log = (output / name / "stderr.log").read_text(encoding="utf-8")
        assert 'Insert instance layer "VK_LAYER_KHRONOS_validation"' in log
        assert "VUID-" not in log and "SYNC-HAZARD" not in log
    report = dict(playerSHA256=digest(player), sourceBytes=original_bytes, sourceSHA256=source_hash,
                  activePreparations=observations, cancelledPreparationCommitted=0, closeCommitted=len(committed),
                  sourceHandleReleased=True, sourceBytesPreserved=True, stagingReleased=True,
                  normalExitAndReopen=True, independentPlayers=3, nativeDialogs=3, liveAllocationsAfterExit=0)
    write(output / "verification.json", report)
    print(json.dumps(report, ensure_ascii=False))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("player", "resources", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    verify(args.player, args.resources, args.output)
