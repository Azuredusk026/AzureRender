local published = {}
local actor = {set = function(_, component, field, value)
    assert(component == "azure.animator" and field == "expression")
    published[#published + 1] = value
end}
local host = {find = function(_, node) assert(node == "face"); return actor end}
local track = function(value) return {seconds = 0, loop = false, curves = value} end
local config = {
    node = "face", defaults = {0, .1, 0}, keys = {'"A"', '"B"', '"C"'},
    prefix = '{"schemaVersion":1,"id":"story-face","skeletonFingerprint":"unit","morphs":[',
    nodePath = '"Rig/Face"',
    facial = {["0"] = track({{1,.1}}), ["1"] = track({{2,.9}}), ["2"] = track({{2,.6}})},
    faceBlend = .25,
    silent = track({{1,.2}}), voiced = track({{1,.45},{3,1}}), speakIn = .1, speakOut = .25,
}
local face = new_story_face(host, config)
face:update(.1)
assert(#published == 0)
face:begin({category=18,commands={}}, false)
face:update(0)
assert(published[#published]:find('"targetId":"A","weight":0.2', 1, true))
face:expression({faceMotion=1})
face:update(0)
assert(not published[#published]:find('"targetId":"B"', 1, true))
face:update(.125)
assert(published[#published]:find('"targetId":"B","weight":0.5', 1, true))
face:expression({faceMotion=2})
face:update(.125)
assert(published[#published]:find('"targetId":"B","weight":0.9', 1, true))
face:update(.01)
face:update(.125)
assert(published[#published]:find('"targetId":"B","weight":0.75', 1, true))
face:voice(true)
face:update(0)
face:update(.05)
assert(published[#published]:find('"targetId":"C","weight":0.5', 1, true))
face:expression({faceMotion=-1})
face:finish()
for _ = 1, 10 do face:update(.1) end
assert(published[#published] == "")
