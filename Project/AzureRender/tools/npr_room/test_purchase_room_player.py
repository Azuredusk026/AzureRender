"""Exercise source-priced ambience purchasing using the normal room and disposable profiles."""

import argparse
import json
from pathlib import Path

from test_ambient_room_player import opened_ambient, preferences
from test_notes_player import playing
from test_story_player import slot
from test_unlock_room_player import save


CONFIRM, CANCEL = "ambient-purchase-confirm", "ambient-purchase-cancel"


def expect_hidden(click, target):
    try:
        click(target)
    except AssertionError as failure:
        response, = failure.args
        assert response["code"] == "InvalidRequest"
        assert response["message"] in ("Game UI target has no pointer interaction",
                                       "Game UI target is outside the viewport"), response
    else:
        raise AssertionError(f"Unexpectedly clickable: {target}")


def replay(player, project, resources, output):
    output.mkdir()
    scarce, funded = output / "scarce-profile", output / "funded-profile"
    report = json.loads(project.with_name("purchase-room-receipt.json").read_bytes())
    assert report["environmentPrices"]["City"] == 14000 and report["ambientOffers"]["City"] == 25
    ambient = json.loads(project.with_name("ambient-receipt.json").read_bytes())
    city, = (row["uuid"] for row in ambient["sounds"] if row["id"] == 25)
    save(scarce, project, "work-calendar", [2, [], [12, 0, 13999], 0])
    save(funded, project, "work-calendar", [2, [], [12, 0, 15000], 0])

    def choose_city(click):
        click("ambient-toggle")
        for _ in range(3):
            click("ambient-page-next")
        click("ambient-select-25")

    with playing(player, project, resources, scarce, output / "insufficient", 1) as (click, _, _, screenshot):
        choose_city(click)
        expect_hidden(click, CONFIRM)
        screenshot("insufficient.png")
        click(CANCEL)
        assert slot(scarce, "room-purchases") is None
        assert slot(scarce, "work-calendar")[2][2] == 13999
    assert not opened_ambient(output / "insufficient")

    with playing(player, project, resources, funded, output / "purchase", 3) as (click, _, _, screenshot):
        choose_city(click)
        screenshot("confirm.png")
        click(CANCEL)
        assert slot(funded, "room-purchases") is None
        click("ambient-select-25")

        home, = funded.glob("project-*")
        ledger = home / "save-room-purchases.json"
        future = b'{"schemaVersion":2,"payload":["preserve"]}'
        ledger.write_bytes(future)
        click(CONFIRM)
        assert ledger.read_bytes() == future
        assert slot(funded, "ambient") is None
        assert slot(funded, "work-calendar")[2][2] == 15000
        screenshot("retry.png")
        ledger.unlink()

        click(CONFIRM)
        assert slot(funded, "room-purchases") == [1, [["City", 14000]]]
        assert preferences(funded) == [1, [[25, 50, False]]]
        assert slot(funded, "work-calendar")[2][2] == 15000
        expect_hidden(click, CONFIRM)
        screenshot("purchased.png")
    assert opened_ambient(output / "purchase") == [city]

    with playing(player, project, resources, funded, output / "restart", 2) as (click, _, _, screenshot):
        choose_city(click)
        expect_hidden(click, CONFIRM)
        assert slot(funded, "room-purchases") == [1, [["City", 14000]]]
        assert preferences(funded) == [1, [[25, 50, False]]]
        screenshot("restarted.png")
        click("ambient-mute")
        assert preferences(funded) == [1, [[25, 50, True]]]
        click("ambient-select-29")
        expect_hidden(click, CONFIRM)
        click(CANCEL)
        assert slot(funded, "room-purchases") == [1, [["City", 14000]]]
    assert opened_ambient(output / "restart") == [city]

    result = {"independentPlayers": 3, "insufficientPoints": 13999, "canceled": True,
              "failedWritePreservedAndRetried": True, "earned": 15000, "spent": 14000,
              "balance": 1000, "purchases": slot(funded, "room-purchases"), "ambientSource": city,
              "audioStarts": [1, 3, 2], "cleanAllocatorExits": 3}
    (output / "purchase-replay.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("player", "project", "resource-root", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(replay(args.player.resolve(), args.project.resolve(), args.resource_root.resolve(),
                            args.output.resolve())))
