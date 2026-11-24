"""Replay original purchases and camera tracks while optional feedback audio fails."""
import argparse
import json
import os
from pathlib import Path
from test_original_ambient_player import fixture as ambient_fixture
from test_original_music_player import digest, write
from test_notes_player import playing

CONFIG = '''
local wallet_catalog={Original={0,""},Another={0,""}}
local environment_access={[1]={"purchase",0,"","Original"},[2]={"purchase",0,"","Another"}}
'''
CAMERA = '''
local ambientInit,ambientUpdate=init,update
local track
function init()
    ambientInit()
    track=new_story_camera(self,{node="source-camera",cueNode="camera-cue",sampleRate=2,
        translation={1,2,3},rotations={{0,0,0},{0,15,0},{0,30,0}},finish={0,45,0}})
end
function update(dt)
    ambientUpdate()
    if self:pressed("camera-start") then track:effect({command="DirectionGameStartCamera"}) end
    if self:pressed("camera-finish") then
        track:finish()
        local actor=self:find("source-camera")
        local p=actor:get("azure.transform","translation")
        local r=actor:get("azure.transform","rotation")
        assert(self:try_write_save("camera-proof",{1,p[1],p[2],p[3],r[1],r[2],r[3]}))
    end
    track:update(dt)
end
'''


def verify(player, resources, source, output):
    os.environ["VK_INSTANCE_LAYERS"]="VK_LAYER_KHRONOS_validation"
    os.environ["VK_LOADER_DEBUG"]="layer"
    output.mkdir()
    project=ambient_fixture(output/"fixture",source/"ambient.lua")
    assets=project.parent/"assets"
    controller=(assets/"ambient.lua").read_text(encoding="utf-8").replace("assets:/tone-1.wav","assets:/tone-2.wav")
    prefix=CONFIG+"\n"+"\n".join((source/n).read_text(encoding="utf-8") for n in ("room-wallet.lua","ambient-access.lua","story-camera.lua"))
    (assets/"ambient.lua").write_text(prefix+controller+CAMERA,encoding="utf-8")
    ui=(assets/"ambient.rml").read_text(encoding="utf-8")
    purchase='''<div id="ambient-purchase-balance"></div><div id="ambient-purchase">
<div id="ambient-purchase-price"></div><button id="ambient-purchase-confirm" data-action="ambient-purchase-confirm">Confirm</button>
<button id="ambient-purchase-cancel" data-action="ambient-purchase-cancel">Cancel</button></div>'''
    ui=ui.replace('<span id="ambient-status"></span>',purchase+'<span id="ambient-status"></span>')
    ui=ui.replace('</body>','''<div style="position:absolute;left:420dp;top:16dp;">
<button id="camera-start" data-action="camera-start">Camera track</button>
<button id="camera-finish" data-action="camera-finish">Finish track</button></div></body>''')
    (assets/"ambient.rml").write_text(ui,encoding="utf-8")
    descriptor=json.loads(project.read_bytes())
    descriptor["runtime"]["input"].update({n:[] for n in ("ambient-purchase-confirm","ambient-purchase-cancel","camera-start","camera-finish")})
    write(project,descriptor)
    level=json.loads((assets/"room.azurelevel").read_bytes())
    level["nodes"].append({"id":"source-camera","components":{}})
    for name in ("room-buy-item","camera-cue"):
        level["nodes"].append({"id":name,"components":{"azure.audio-source":{
            "type":"azure.audio-source","version":1,"data":{"asset":"assets:/tone-1.wav","autoplay":False,"loop":False,"volume":.4}}}})
    write(assets/"room.azurelevel",level)
    tone=assets/"tone-1.wav"; original=tone.read_bytes();tone.write_bytes(original[:16])
    profile,control=output/"profile",{}
    def read(slot):
        p,=profile.glob(f"project-*/save-{slot}.json")
        return json.loads(p.read_bytes())["payload"]
    try:
        with playing(player,project,resources,profile,output/"recover",4,
                     on_ready=lambda _,call:control.update(call=call),
                     expected_presentation_errors=("Truncated WAV container",)) as (click,_,_,screenshot):
            click("camera-start");control["call"]("wait-frames",frames=90);click("camera-finish")
            assert read("camera-proof")==[1,1,2,3,0,45,0]
            click("ambient-toggle");click("ambient-select-1");click("ambient-purchase-confirm")
            assert read("room-purchases")==[1,[["Original",0]]]
            assert read("ambient")==[1,[[1,50,False]]]
            assert json.loads(control["call"]("query",name="game.state"))["audioStarts"]==1
            screenshot("purchase-without-cue.png")
            tone.write_bytes(original)
            click("ambient-select-2");click("ambient-purchase-confirm")
            assert read("room-purchases")==[1,[["Original",0],["Another",0]]]
            assert read("ambient")==[1,[[1,50,False],[2,50,False]]]
            click("camera-start");control["call"]("wait-frames",frames=90);click("camera-finish")
            assert read("camera-proof")==[1,1,2,3,0,45,0]
            screenshot("cues-recovered.png")
    finally:
        tone.write_bytes(original)
    with playing(player,project,resources,profile,output/"reopen",2) as (click,_,_,screenshot):
        click("ambient-toggle");click("ambient-mute")
        assert read("ambient")==[1,[[1,50,True],[2,50,False]]]
        assert read("room-purchases")==[1,[["Original",0],["Another",0]]]
        screenshot("reopened.png")
    for stage in ("recover","reopen"):
        log=(output/stage/"stderr.log").read_text(encoding="utf-8")
        assert 'Insert instance layer "VK_LAYER_KHRONOS_validation"' in log
        assert "VUID-" not in log and "SYNC-HAZARD" not in log
    result={"playerSHA256":digest(player),"sourceSHA256":{n:digest(source/n) for n in ("ambient.lua","story-camera.lua")},
        "cameraPose":read("camera-proof"),"purchases":read("room-purchases"),"ambient":read("ambient"),
        "independentPlayers":2,"liveAllocationsAfterExit":0}
    write(output/"verification.json",result);print(json.dumps(result))


if __name__ == "__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ("player","resources","source","output"):
        parser.add_argument("--"+name,type=Path,required=True)
    args=parser.parse_args();verify(args.player,args.resources,args.source,args.output)
