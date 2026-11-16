"""Stage the Recovery Windows music library and room controls."""

import argparse
from html import escape
import json
from pathlib import Path
import re
import shutil
import tempfile
from uuid import UUID

from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest
from tools.npr_room.room_geometry import activate_room_control, copy_room_asset, stage_live_audio_media
from tools.npr_room.skinned_mesh_export import UnityScene


MUSIC_MASTER = "Assets/App/BulbulAssets/MasterData/ScriptableObjects/MusicMaster.asset"
MUSIC_ACTIONS = ("toggle", "play", "previous", "next", "repeat", "shuffle", "quiet", "loud", "mute", "list",
                 "page-prev", "page-next")
SIZE_TAG = re.compile(r"</?size(?:=[\d.]+%?)?>", re.IGNORECASE)


def music_catalog(source, audio_catalog, audio_probe):
    master = source / MUSIC_MASTER
    slots = json.loads(audio_catalog.read_bytes())["slots"]
    playback = json.loads(audio_probe.read_bytes())["files"]
    tracks = []
    seen = set()
    for row in UnityScene(master).read(11400000)["AudioCollection"]:
        clip = row["AudioClip"]
        guid = UUID(clip["guid"]).hex
        uuid = str(UUID(row["UUID"]))
        if clip["fileID"] != 8300000 or clip["type"] != 3 or uuid in seen:
            raise ValueError(f"Invalid music reference identity: {uuid}")
        seen.add(uuid)
        entry = slots[guid]
        if Path(entry["source"]).suffix.lower() != ".ogg":
            raise ValueError(f"Expected original Windows music container: {uuid}")
        samples = playback[entry["sha256"] + ".ogg"]
        frames, sample_rate = samples["frames"], samples["sampleRate"]
        if frames <= 0 or sample_rate <= 0:
            raise ValueError(f"Invalid original music duration: {uuid}")
        title, credit = row["Title"], row["Credit"]
        if not title or any(ord(character) < 32 for character in title + credit):
            raise ValueError(f"Invalid music display text: {uuid}")
        tracks.append({"uuid": uuid, "guid": guid, "fileID": clip["fileID"], "tag": row["Tag"],
                       "title": title, "credit": credit, "locked": bool(row["IsManualUnlock"]),
                       "displayTitle": SIZE_TAG.sub("", title), "displayCredit": SIZE_TAG.sub("", credit),
                       "source": entry["source"], "sha256": entry["sha256"], "asset": f"music/{uuid}.ogg",
                       "frames": frames, "sampleRate": sample_rate})
    if not any(not row["locked"] for row in tracks):
        raise ValueError("The Windows music master has no available tracks")
    return {"schemaVersion": 1, "sourceSHA256": digest(master), "tracks": tracks}


def music_config(playlist):
    rows = []
    for row in playlist["tracks"]:
        if not row["locked"]:
            values = (row["uuid"], "assets:/" + row["asset"], row["displayTitle"], row["displayCredit"])
            values = ", ".join(json.dumps(value, ensure_ascii=False) for value in values)
            rows.append(f"    {{{values}, {row['frames']} / {row['sampleRate']}}},")
    return "local tracks = {\n" + "\n".join(rows) + "\n}\n"


def stage_music(room, output, source, audio_catalog, audio_probe):
    example = Path(__file__).resolve().parents[2] / "examples/npr-room/assets"
    playlist = music_catalog(source, audio_catalog, audio_probe)
    available = [row for row in playlist["tracks"] if not row["locked"]]
    selections = [f"select-{index}" for index in range(1, len(available) + 1)]
    descriptor = json.loads((room / "project.azureproject").read_bytes())

    with tempfile.TemporaryDirectory(prefix="room-music-", dir=output.parent) as parent:
        project = Path(parent) / "project"
        assets = project / "assets"
        shutil.copytree(room / "assets", assets, copy_function=copy_room_asset, ignore=shutil.ignore_patterns(".azure"))
        level_path, ui_path = assets / "room.azurelevel", assets / "timer.rml"
        level = json.loads(level_path.read_bytes())
        ui = ui_path.read_text(encoding="utf-8")
        activate_room_control(descriptor, level, "music", (*MUSIC_ACTIONS, *selections))
        level["nodes"][-1]["components"]["azure.audio-source"] = {
            "type": "azure.audio-source", "version": 1, "data": {"volume": .5},
        }
        listing = "\n".join(
            f'<button id="music-{action}" data-action="music-{action}">{escape(row["displayTitle"])}</button>'
            for action, row in zip(selections, available, strict=True)
        )
        controls = (example / "music.rml").read_text(encoding="utf-8").replace("{{tracks}}", listing)
        style, body = controls.split("</style>\n", 1)
        ui = ui.replace("</style>", style + "</style>", 1).replace("</body>", body + "</body>", 1)
        stage_live_audio_media(source, playlist["tracks"], assets)
        write(project / "project.azureproject", descriptor)
        write(project / "music-receipt.json", playlist)
        write(level_path, level)
        ui_path.write_text(ui, encoding="utf-8", newline="\n")
        script = music_config(playlist) + (example / "music.lua").read_text(encoding="utf-8")
        (assets / "music.lua").write_text(script, encoding="utf-8", newline="\n")
        publish_directory(project, output)
    return {"tracks": len(playlist["tracks"]), "available": len(available),
            "sourceSHA256": playlist["sourceSHA256"]}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("room", "output", "source", "audio-catalog", "audio-probe"):
        parser.add_argument("--" + argument, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(stage_music(args.room, args.output, args.source, args.audio_catalog, args.audio_probe)))
