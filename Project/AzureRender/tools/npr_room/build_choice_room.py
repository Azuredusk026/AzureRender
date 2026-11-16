"""Stage source work conversations in the existing room project."""

import argparse
import json
from pathlib import Path
import shutil
import tempfile

from tools.npr_room.build_voice_room import voice_config, voice_line_config
from tools.npr_room.build_work_calendar import calendar_script, work_rewards
from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.choice_story import choice_parameters, choice_program
from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest
from tools.npr_room.room_geometry import copy_room_asset, stage_live_audio_media


def choice_config(episodes, parameters, language, seed=None):
    code = []
    if seed is not None:
        code.append(f"math.randomseed({seed})")
    code.append("local work_choice_parameters = {")
    for key in ("probability", "fullProbabilitySeconds", "availableSeconds"):
        code.append(f"    {key} = {parameters[key]},")
    code.extend(("}", "local work_conversations = {"))
    for episode in episodes:
        intro = voice_line_config(episode["intro"], language)
        code.append(f"    {{{intro}, {{")
        for option in episode["options"]:
            label = json.dumps(option["label"][language], ensure_ascii=False)
            line = voice_line_config(option["response"], language)
            code.append(f"        {{{label}, {line}}},")
        code.append("    }},")
    code.append("}")
    return "\n".join(code) + "\n"


def stage_choice(room, output, source, inventory, audio_catalog, audio_probe, seed=None):
    example = Path(__file__).resolve().parents[2] / "examples/npr-room/assets"
    story_hash = digest(inventory)
    voice = json.loads((room / "voice-receipt.json").read_bytes())
    if voice["storySHA256"] != story_hash:
        raise ValueError("The room's voice data and conversation inventory use distinct sources")
    language = voice["language"]
    episodes, media = choice_program(json.loads(inventory.read_bytes()),
                                    json.loads(audio_catalog.read_bytes())["slots"],
                                    json.loads(audio_probe.read_bytes())["files"])
    parameters = choice_parameters(source)
    rewards = work_rewards(source)
    if any(len(episode["options"]) != 3 for episode in episodes):
        raise ValueError("The room's conversation panel requires three original choices")
    descriptor = json.loads((room / "project.azureproject").read_bytes())
    actions = {f"room-choice-option-{index}": [] for index in range(1, 4)}
    if descriptor["runtime"]["input"].keys() & actions.keys():
        raise ValueError("This room already has work conversations")
    descriptor["runtime"]["input"].update(actions)
    with tempfile.TemporaryDirectory(prefix="room-choice-", dir=output.parent) as parent:
        project = Path(parent) / "project"
        assets = project / "assets"
        shutil.copytree(room / "assets", assets, copy_function=copy_room_asset, ignore=shutil.ignore_patterns(".azure"))
        level_path, ui_path = assets / "room.azurelevel", assets / "timer.rml"
        level = json.loads(level_path.read_bytes())
        level["nodes"].append({"id": "room-choice-voice", "components": {
            "azure.audio-source": {"type": "azure.audio-source", "version": 1, "data": {"volume": .5}},
        }})
        ui = ui_path.read_text(encoding="utf-8")
        style, body = (example / "work-choice.rml").read_text(encoding="utf-8").split("</style>\n", 1)
        ui = ui.replace("</style>", style + "</style>", 1).replace("</body>", body + "</body>", 1)
        script = [voice_config(voice["responses"], language).encode("utf-8"),
                  choice_config(episodes, parameters, language, seed).encode("utf-8"),
                  (example / "room-voice.lua").read_bytes(), (example / "work-choice.lua").read_bytes(),
                  calendar_script(example, rewards)]
        stage_live_audio_media(source, media, assets)
        write(project / "project.azureproject", descriptor)
        write(project / "choice-receipt.json", {"schemaVersion": 1, "storySHA256": story_hash, "language": language,
                                               "parameters": parameters, "seed": seed, "episodes": episodes, "media": media})
        for receipt in ("music-receipt.json", "ambient-receipt.json", "voice-receipt.json", "work-progression-receipt.json"):
            shutil.copyfile(room / receipt, project / receipt)
        write(level_path, level)
        ui_path.write_text(ui, encoding="utf-8", newline="\n")
        (assets / "pomodoro.lua").write_bytes(b"\n".join(script))
        publish_directory(project, output)
    return {"episodes": len(episodes), "voiceSources": len(media)}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("room", "source", "output", "inventory", "audio-catalog", "audio-probe"):
        parser.add_argument("--" + argument, type=Path, required=True)
    parser.add_argument("--seed", type=int)
    args = parser.parse_args()
    print(json.dumps(stage_choice(args.room, args.output, args.source, args.inventory, args.audio_catalog,
                                  args.audio_probe, args.seed)))
