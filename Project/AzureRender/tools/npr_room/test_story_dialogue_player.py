"""Check original opening voice media and caption language on the native Player."""

import argparse
from contextlib import contextmanager
import json
from pathlib import Path
import time

import numpy as np
from PIL import Image

from reference_inventory import digest
from test_notes_player import playing
from test_room_voice_player import opened_voice
from tools.npr_room.room_language import FIELDS
from tools.npr_room.story_chapters import lua_value


@contextmanager
def dialogue_fixture(room, lines, language):
    definition = json.loads((room / "project.azureproject").read_bytes())
    scene = json.loads((room / "assets/room.azurelevel").read_bytes())
    temporary = "capture-opening-dialogue"
    definition["startupScene"] = f"assets:/{temporary}.azurelevel"
    definition["runtime"]["input"].update({label: [] for label in ("opening-story-lesson", "opening-story-chapter")})
    for identity, kind, data in (
        ("opening-story-voice", "audio-source", {"volume": .5}),
        ("opening-story-fixture", "script", {"asset": f"assets:/{temporary}.lua"}),
    ):
        component = "azure." + kind
        scene["nodes"].append({"id": identity, "components": {component: {"type": component, "version": 1, "data": data}}})
    for node in scene["nodes"]:
        component = node["components"].get("azure.game-ui")
        if component and component["data"]["asset"] == "assets:/timer.rml":
            component["data"]["asset"] = f"assets:/{temporary}.rml"
    config = (room / "assets/opening-dialogue-program.lua").read_text(encoding="utf-8")
    fixture_script = config + "\nlocal rows = " + lua_value(lines) + "\nlocal tags = " + lua_value(list(FIELDS)) + f"\nlocal fallback = {language}\n"
    fixture_script += """
local dialogue, language, pendingLanguage
local function currentLanguage()
    local value = self:read_save("room-language")
    if type(value) == "table" and value[1] == 1 and tags[value[2]] then return tags[value[2]] end
    return tags[fallback]
end
function init()
    dialogue = new_story_dialogue(self, opening_caption_speeds, function() return language end)
    language = currentLanguage()
end
function update(dt)
    if pendingLanguage then
        language = currentLanguage()
        dialogue:languageChanged()
    end
    pendingLanguage = false
    for index in ipairs(tags) do
        if self:pressed("room-language-" .. index) then pendingLanguage = true end
    end
    if self:pressed("opening-story-lesson") then dialogue:show(rows[1]) end
    if self:pressed("opening-story-chapter") then dialogue:show(rows[2]) end
    dialogue:update(dt)
end
"""
    markup = (room / "assets/timer.rml").read_text(encoding="utf-8")
    style = """
#opening-story-caption { position: absolute; left: 25%; bottom: 90dp; width: 50%; height: 70dp; padding: 14dp; background-color: #19232c; font-size: 24dp; white-space: pre-wrap; }
#opening-story-lesson, #opening-story-chapter { position: absolute; left: 92dp; bottom: 16dp; padding: 6dp 10dp; color: white; background-color: #34434f; }
#opening-story-chapter { left: 181dp; }
"""
    content = """
<div id="opening-story-caption" style="display: none;"><span id="opening-story-line"></span></div>
<button id="opening-story-lesson" data-action="opening-story-lesson">第0话</button>
<button id="opening-story-chapter" data-action="opening-story-chapter">第1话</button>
"""
    markup = markup.replace("</style>", style + "</style>", 1).replace("</body>", content + "</body>", 1)
    files = {room / f"{temporary}.azureproject": json.dumps(definition),
             room / "assets" / f"{temporary}.azurelevel": json.dumps(scene),
             room / "assets" / f"{temporary}.lua": fixture_script,
             room / "assets" / f"{temporary}.rml": markup}
    created = []
    try:
        for path, data in files.items():
            with path.open("x", encoding="utf-8") as file:
                created.append(path)
                file.write(data)
        yield next(iter(files)), definition["startupScene"]
    finally:
        for path in reversed(created):
            path.unlink()


def frame(screenshot, folder, name):
    screenshot()
    result = folder / (name + ".png")
    (folder / "notes.png").rename(result)
    return result


def replay(player, room, resources, chapter_path, output):
    output.mkdir()
    receipt = json.loads((room / "opening-dialogue-receipt.json").read_bytes())
    assert receipt["sourceSHA256"] == digest(chapter_path) and receipt["lineCount"] == 33
    source = json.loads(chapter_path.read_bytes())
    lines = [row for chapter in source["chapters"] for row in chapter["commands"] if row["row"] in (30, 96)]
    expected = [Path(row["asset"]).name for row in lines]
    for row in lines:
        assert digest(room / "assets" / row["asset"]) == receipt["voiceSHA256"][row["asset"]]
    folder, profile = output / "room", output / "profile"
    with dialogue_fixture(room, lines, receipt["languageDefault"]) as (project, level):
        with playing(player, project, resources, profile, folder, expected_audio_starts=3, level_reference=level) as (click, _, _, screenshot):
            click("opening-story-lesson")
            time.sleep(.42)
            first = frame(screenshot, folder, "lesson-chinese")
            click("room-language-toggle")
            click("room-language-7")
            click("room-language-toggle")
            time.sleep(.28)
            second = frame(screenshot, folder, "lesson-russian")
            assert opened_voice(folder) == expected[:1]
            click("opening-story-chapter")
            assert opened_voice(folder) == expected
            time.sleep(.42)
            third = frame(screenshot, folder, "chapter-russian")
    files = list(profile.glob("project-*/save-room-language.json"))
    assert len(files) == 1 and json.loads(files[0].read_bytes())["payload"] == [1, 7]

    def caption(path):
        with Image.open(path) as image:
            # The fixture renders its first baseline within this opaque caption box.
            box = np.asarray(image.convert("RGB"))[455:500, 335:650]
            assert np.all(box[:, 0] == [25, 35, 44])
            return box

    changed = int(np.count_nonzero(np.any(caption(first) != caption(second), axis=2)))
    assert changed > 100, changed
    chapter_changed = int(np.count_nonzero(np.any(caption(second) != caption(third), axis=2)))
    assert chapter_changed > 100, chapter_changed
    report = {"originalRows": [row["row"] for row in lines], "nativeVoiceFiles": expected, "audioStarts": 3,
              "captionPixelsChanged": changed, "chapterCaptionPixelsChanged": chapter_changed,
              "frameSHA256": [digest(path) for path in (first, second, third)]}
    (output / "dialogue-replay.json").write_text(json.dumps(report) + "\n", encoding="utf-8")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for arg in ("player", "room", "resource-root", "chapters", "output"):
        parser.add_argument("--" + arg, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(replay(args.player.resolve(), args.room.resolve(), args.resource_root.resolve(),
                            args.chapters.resolve(), args.output.resolve())))
