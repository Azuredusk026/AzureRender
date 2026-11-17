"""Replay the opening chapter controls, original captions, and completion in native Players."""

import argparse
from contextlib import contextmanager
import json
from pathlib import Path
import time

from reference_inventory import digest
from test_notes_player import playing
from test_room_voice_player import opened_voice
from tools.npr_room.room_language import FIELDS
from tools.npr_room.story_chapters import lua_value, translations


EXAMPLE = Path(__file__).resolve().parents[2] / "examples/npr-room"
PREFIX = "opening-story-"


@contextmanager
def route_fixture(room, chapters, inventory):
    receipt = json.loads((room / "opening-dialogue-receipt.json").read_bytes())
    story = json.loads(chapters.read_bytes())
    playlist = json.loads((room / "music-receipt.json").read_bytes())["tracks"]
    music_titles = {command["arguments"][0] for chapter in story["chapters"] for command in chapter["commands"]
                    if command["command"] in ("PlayBGM", "TalkLogOnlyPlayBGM")}
    music = {}
    for title in music_titles:
        asset, = (track for track in playlist if track["title"] == title)
        assert digest(room / "assets" / asset["asset"]) == asset["sha256"]
        music[title] = asset["asset"]
    assert receipt["sourceSHA256"] == digest(chapters)
    assert story["sourceSHA256"]["inventory"] == digest(inventory)
    project = json.loads((room / "project.azureproject").read_bytes())
    scene = json.loads((room / "assets/room.azurelevel").read_bytes())
    name = "capture-story-route"
    level = f"assets:/{name}.azurelevel"
    project["startupScene"] = level
    control_names = ["start", "log", "next", "auto", "heroine",
                     *(f"option-{index}" for index in range(1, 5)),
                     *(f"replay-{index}" for index in range(1, 3))]
    project["runtime"]["input"].update({PREFIX + label: [] for label in control_names})
    for identifier, type_name, data in (
        ("opening-story-voice", "azure.audio-source", {"volume": .5}),
        ("opening-story-music", "azure.audio-source", {"volume": 0, "loop": True}),
        ("opening-story-noise", "azure.screen-noise", {}),
        ("opening-story-route", "azure.script", {"asset": f"assets:/{name}.lua"}),
    ):
        scene["nodes"].append({"id": identifier, "components": {type_name: {"type": type_name, "version": 1, "data": data}}})
    for node in scene["nodes"]:
        ui = node["components"].get("azure.game-ui")
        if ui and ui["data"]["asset"] == "assets:/timer.rml":
            ui["data"]["asset"] = f"assets:/{name}.rml"
    localized = {row["ID"]: row for row in json.loads(inventory.read_bytes())["localization"]}
    config = {
        "room_story_chapters": story["chapters"], "opening_tags": list(FIELDS),
        "opening_language_default": receipt["languageDefault"],
        "opening_music": {title: f"assets:/{asset}" for title, asset in music.items()},
        "opening_labels": {key: translations(localized[label]) for key, label in
                           (("log", "ui_story_title"), ("auto-label", "ui_scenario_auto"))},
    }
    source = "\n".join(f"local {key} = {lua_value(value)}" for key, value in config.items()).encode("utf-8")
    code = b"\n".join((source, (room / "assets/opening-dialogue-program.lua").read_bytes(),
                       *((EXAMPLE / "assets" / item).read_bytes() for item in
                         ("story-progress.lua", "story-reader.lua", "story-player.lua", "story-music.lua", "story-noise.lua")),
                       (EXAMPLE / "tests/story-player-native.lua").read_bytes()))
    template = (EXAMPLE / "assets/story-player.rml").read_text(encoding="utf-8")
    style, controls = template.split("</style>", 1)
    style += "#opening-tutorial-clock { position: absolute; right: 14dp; top: 14dp; width: 270dp; height: 90dp; border: 2dp solid #f0c58e; pointer-events: none; }"
    controls += '<div id="opening-tutorial-clock" style="display: none;"></div>'
    markup = (room / "assets/timer.rml").read_text(encoding="utf-8")
    assert markup.count("</style>") == 1 and markup.count("</body>") == 1
    markup = markup.replace("</style>", style + "</style>").replace("</body>", controls + "</body>")
    artifacts = {
        room / f"{name}.azureproject": json.dumps(project).encode("utf-8"),
        room / "assets" / f"{name}.azurelevel": json.dumps(scene).encode("utf-8"),
        room / "assets" / f"{name}.lua": code,
        room / "assets" / f"{name}.rml": markup.encode("utf-8"),
    }
    created = []
    try:
        for path, data in artifacts.items():
            with path.open("xb") as opened:
                created.append(path)
                opened.write(data)
        yield next(iter(artifacts)), level, story["chapters"], {"playlist": playlist[0]["asset"], **music}
    finally:
        for path in reversed(created):
            path.unlink()


def slot(profile, name):
    files = list(profile.glob(f"project-*/save-{name}.json"))
    if not files:
        return None
    path, = files
    data = json.loads(path.read_bytes())
    assert data["schemaVersion"] == 1
    return data["payload"]


def wait_for_slot(profile, name, predicate, wait):
    deadline = time.monotonic() + wait
    while time.monotonic() < deadline:
        value = slot(profile, name)
        if value is not None and predicate(value):
            return value
        time.sleep(.25)
    raise AssertionError(f"{name} was {slot(profile, name)}")


def assert_locked(click):
    try:
        click(PREFIX + "start")
    except AssertionError as rejected:
        reply, = rejected.args
        assert reply["code"] == "InvalidRequest"
        assert reply["message"] in ("Game UI target is outside the viewport", "Game UI target has no pointer interaction")
    else:
        raise AssertionError("Level one unexpectedly exposed the first chapter")


def compare_voices(folder, chapters, expected_rows):
    source = {Path(row["asset"]).name: row["row"] for chapter in chapters for row in chapter["commands"] if row.get("asset")}
    actual = [source[file] for file in opened_voice(folder) if file in source]
    assert actual == expected_rows, actual


def opened_music(folder):
    lines = (folder / "stderr.log").read_text(encoding="utf-8").splitlines()
    return [Path(line.rsplit(" source: ", 1)[1]).name
            for line in lines if " source: " in line and "assets\\music\\" in line]


@contextmanager
def normal_room_fixture(room, chapters, inventory):
    receipt = json.loads((room / "opening-story-room-receipt.json").read_bytes())
    project = room / "project.azureproject"
    assert receipt["chaptersSHA256"] == digest(chapters) and receipt["inventorySHA256"] == digest(inventory)
    assert receipt["startupScene"] == json.loads(project.read_bytes())["startupScene"] == "assets:/boot.azurelevel"
    assert receipt["roomScene"] == "assets:/room.azurelevel"
    for asset, field in (("room.azurelevel", "levelSHA256"), ("pomodoro.lua", "scriptSHA256"),
                         ("timer.rml", "documentSHA256")):
        assert digest(room / "assets" / asset) == receipt[field]
    playlist = json.loads((room / "music-receipt.json").read_bytes())["tracks"]
    yield project, receipt["roomScene"], json.loads(chapters.read_bytes())["chapters"], {
        "playlist": playlist[0]["asset"], **receipt["tracks"]}


def replay(player, room, resources, chapters, inventory, output, normal=False):
    output.mkdir()
    profile = output / "profile"
    tutorial = output / "tutorial"
    chapter = output / "chapter"
    fixture = normal_room_fixture if normal else route_fixture
    with fixture(room, chapters, inventory) as (project, level, authored, music):
        files = {label: Path(path).name for label, path in music.items()}
        with playing(player, project, resources, profile, tutorial, level_reference=level) as (click, press, _, screenshot):
            click(PREFIX + "start")
            click(PREFIX + "option-1", wait=70)
            if normal:
                press(77, domain="game")
                assert slot(profile, "timer-mode") is None
            click(PREFIX + "heroine", wait=38)
            for option in (1, 2, 3, 4, 1):
                click(PREFIX + f"option-{option}", wait=85)
            if normal:
                wait_for_slot(profile, "room-unlocks", lambda value: "tutorial_01" in value[1], 40)
                click("notes-toggle", wait=8)
                click("notes-toggle")
            else:
                tutorial_proof = wait_for_slot(profile, "opening-replay-proof", lambda value: value[0] == "tutorial_01", 40)
            assert slot(profile, "room-unlocks")[1] == ["tutorial_01"]
            assert_locked(click)
            screenshot()
        tutorial_rows = [9, 24, 30, 39, 44, 45, 46, 48, 57, 59, 62, 64, 67, 69, 72, 73, 74, 75, 76, 80, 85, 86]
        compare_voices(tutorial, authored, tutorial_rows)
        assert opened_music(tutorial) == [files["playlist"]]
        if not normal:
            assert {58, 63, 68} <= {row for row, _ in tutorial_proof[2]} and not tutorial_proof[1]
            assert [12, .7] in tutorial_proof[2] and [14, .7] in tutorial_proof[2] and [0, 0] not in tutorial_proof[2]
            assert tutorial_proof[3][0] > 0 and abs(tutorial_proof[3][1] - .05) < 1e-6

        with playing(player, project, resources, profile, chapter, level_reference=level) as (click, _, _, screenshot):
            assert_locked(click)
            click("timer-mode-button")
            click("timer-toggle-button")
            time.sleep(1.25)
            click("timer-end-button")
            progress = wait_for_slot(profile, "work-calendar", lambda value: value[0] == 2 and value[2][0] == 2, 12)
            click(PREFIX + "start", wait=10)
            if normal:
                wait_for_slot(profile, "room-unlocks", lambda value: "main_01" in value[1], 110)
            else:
                chapter_proof = wait_for_slot(profile, "opening-replay-proof", lambda value: value[0] == "main_01", 110)
            assert slot(profile, "room-unlocks")[1] == ["tutorial_01", "main_01"]
            chapter_rows = [96, 97, 98, 99, 100, 101, 105, 106, 107, 108]
            compare_voices(chapter, authored, chapter_rows)
            assert opened_music(chapter) == [files["playlist"], files["NEWings"]]
            click(PREFIX + "log", wait=8)
            screenshot()
            click(PREFIX + "replay-1", wait=8)
            deadline = time.monotonic() + 8
            while files["Nostalgia"] not in opened_music(chapter) and time.monotonic() < deadline:
                time.sleep(.25)
            assert slot(profile, "room-unlocks")[1] == ["tutorial_01", "main_01"]
        assert opened_music(chapter) == [files["playlist"], files["NEWings"], files["Nostalgia"]]
        if not normal:
            assert [index for index, _ in chapter_proof[2]] == [90, 91, 92, 93, 95, 102, 104, 109] and not chapter_proof[1]
            assert chapter_proof[3][0] > 0 and .029999 < chapter_proof[3][1] <= .050001
    report = {"launches": 2, "levelFromCountup": progress[2][0], "tutorialVoiceRows": tutorial_rows,
              "chapterVoiceRows": chapter_rows,
              "tutorialMusic": opened_music(tutorial), "chapterAndReplayMusic": opened_music(chapter),
              "playerSHA256": digest(player), "chaptersSHA256": digest(chapters)}
    if normal:
        for launch in (tutorial, chapter):
            warnings = (launch / "stderr.log").read_text(encoding="utf-8").splitlines()
            assert not [line for line in warnings if "[game-ui]" in line and "Syntax error parsing" in line], launch
        report.update(startupScene="assets:/boot.azurelevel", roomScene=level,
                      roomReceiptSHA256=digest(room / "opening-story-room-receipt.json"))
    else:
        report.update(tutorialEffects=tutorial_proof[2], chapterEffects=chapter_proof[2],
                      nonzeroNoiseFramesAndPeak={"tutorial": tutorial_proof[3], "chapter": chapter_proof[3]})
    (output / "opening-route-replay.json").write_text(json.dumps(report) + "\n", encoding="utf-8")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("player", "room", "resource-root", "chapters", "inventory", "output"):
        parser.add_argument("--" + option, type=Path, required=True)
    parser.add_argument("--normal-room", action="store_true")
    args = parser.parse_args()
    print(json.dumps(replay(args.player.resolve(), args.room.resolve(), args.resource_root.resolve(),
                            args.chapters.resolve(), args.inventory.resolve(), args.output.resolve(), args.normal_room)))
