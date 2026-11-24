"""Replay language payload protection with original localized UI and restart."""
import argparse
import json
import os
from pathlib import Path
from test_original_music_player import fixture as music_fixture, digest, write
from test_notes_player import playing
from test_unlock_room_player import save

DRIVER = '''
local language, previous
function init()
    language=new_room_language({default=1,strings={language={"Language","Langue"},
        finish={"Finish","Terminer"},statistics={"Statistics","Statistiques"}}})
end
function update()
    language:update()
    local text=language:text({"Original room","Salle originale"})
    self:ui_text("language-message",text)
    if text~=previous then assert(self:try_write_save("language-proof",{1,text}));previous=text end
end
'''


def verify(player, resources, source, output):
    os.environ["VK_INSTANCE_LAYERS"] = "VK_LAYER_KHRONOS_validation"
    os.environ["VK_LOADER_DEBUG"] = "layer"
    output.mkdir()
    project = music_fixture(output / "fixture", source.with_name("music.lua"))
    assets = project.parent / "assets"
    descriptor = json.loads(project.read_bytes())
    descriptor["runtime"]["input"] = {name: [] for name in ("room-language-toggle","room-language-1","room-language-2")}
    write(project, descriptor)
    (assets / "language.lua").write_text(source.read_text(encoding="utf-8") + DRIVER, encoding="utf-8")
    (assets / "language.rml").write_text('''<rml><head><style>
body {font-family:LatoLatin;margin:36dp;color:white;} #panel {padding:20dp;background-color:#263b45;width:520dp;}
button {padding:8dp;margin:4dp;background-color:#406677;}
</style></head><body><div id="panel"><div id="language-message"></div>
<button id="room-language-toggle" data-action="room-language-toggle">Language</button>
<div id="room-language-panel"><button id="room-language-1" data-action="room-language-1">English</button>
<button id="room-language-2" data-action="room-language-2">French</button></div>
<div id="timer-end-button"></div><div id="timer-stats"></div></div></body></rml>''', encoding="utf-8")
    level = json.loads((assets / "room.azurelevel").read_bytes())
    components = level["nodes"][0]["components"]
    components.pop("azure.audio-source")
    components["azure.script"]["data"]["asset"] = "assets:/language.lua"
    components["azure.game-ui"]["data"]["asset"] = "assets:/language.rml"
    write(assets / "room.azurelevel", level)
    profile = output / "profile"
    save(profile, project, "room-language", [2,"future locale"])
    slot, = profile.glob("project-*/save-room-language.json")
    protected = slot.read_bytes()
    def text():
        path, = profile.glob("project-*/save-language-proof.json")
        return json.loads(path.read_bytes())["payload"][1]
    with playing(player, project, resources, profile, output / "protect", 0) as (click, _, _, screenshot):
        click("room-language-toggle"); click("room-language-2")
        assert slot.read_bytes()==protected and text()=="Original room"
        screenshot("future-protected.png")
        save(profile, project, "room-language", [1,1])
        click("room-language-2")
        assert json.loads(slot.read_bytes())["payload"]==[1,2] and text()=="Salle originale"
        stored=slot.read_bytes()
        external=b'{ "schemaVersion": 1, "payload": [3,"external future locale"] }'
        slot.write_bytes(external);click("room-language-1")
        assert slot.read_bytes()==external and text()=="Salle originale"
        slot.write_bytes(stored);click("room-language-1")
        assert json.loads(slot.read_bytes())["payload"]==[1,1] and text()=="Original room"
        screenshot("compatible-restored.png")
    with playing(player, project, resources, profile, output / "reopen", 0) as (click, _, _, screenshot):
        assert text()=="Original room"
        click("room-language-toggle");click("room-language-2")
        assert text()=="Salle originale" and json.loads(slot.read_bytes())["payload"]==[1,2]
        screenshot("reopened.png")
    for stage in ("protect","reopen"):
        log=(output/stage/"stderr.log").read_text(encoding="utf-8")
        assert 'Insert instance layer "VK_LAYER_KHRONOS_validation"' in log
        assert "VUID-" not in log and "SYNC-HAZARD" not in log
    result={"playerSHA256":digest(player),"controllerSHA256":digest(source),"protectedVersions":[2,3],
        "protectedOriginalBytes":True,"reopenedText":text(),"independentPlayers":2,"liveAllocationsAfterExit":0}
    write(output/"verification.json",result);print(json.dumps(result))


if __name__ == "__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ("player","resources","source","output"):
        parser.add_argument("--"+name,type=Path,required=True)
    args=parser.parse_args();verify(args.player,args.resources,args.source,args.output)
