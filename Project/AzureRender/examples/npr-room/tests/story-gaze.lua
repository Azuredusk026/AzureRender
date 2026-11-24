local actor = {}
local active = ""
function actor:set(component, field, value)
    assert(component == "azure.animator" and field == "expression")
    active = value
end
local host = {}
function host:find(node) assert(node == "face"); return actor end
local track = function(curves) return {seconds = 0, loop = false, curves = curves} end
local config = {node = "face", keys = {'"Eye"'}, defaults = {0}, nodePath = '"Head/Face"',
    prefix = '{"schemaVersion":1,"morphs":[',
    facial = {["0"] = track({}), ["1"] = track({{1,.1}})}, faceBlend = .25,
    silent = track({}), voiced = track({}), speakIn = .1, speakOut = .25,
    look = {seconds = 1, eyes = {{1, .25}},
        jointCurves = {{.15, .25, .35}}, targets = {{node = "face",
            prefix = '{"schemaVersion":2,"humanoidRotations":[',
            joints = {{index = 1, prefix = '{"path":"Head","offset":['}}}}}
}
local face = new_story_face(host, config)
face:begin({category=18,commands={}}, false)
face:expression({faceMotion=0,look={scale=1,seconds=.5,ease=0}})
face:update(.25)
assert(active:find('"offset":[0.15,0.25,0.35]', 1, true))
assert(active:find('"targetId":"Eye","weight":0.25,"additive":true', 1, true))
face:expression({faceMotion=1})
face:update(.1)
face:update(.125)
assert(active:find('"targetId":"Eye","weight":0.05', 1, true))
assert(not active:find('"additive":true', 1, true))
face:finish({category=18}, false)
for _ = 1, 60 do face:update(1/60) end
assert(active:find('"humanoidRotations"', 1, true))
for _ = 1, 30 do face:update(1/60) end
assert(active == "")
