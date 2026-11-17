local nodes = {face = {pose = ""}, body = {pose = ""}}
for _, actor in pairs(nodes) do
    function actor:set(component, field, value)
        assert(component == "azure.animator" and field == "expression")
        self.pose = value
    end
end
local host = {}
function host:find(node) return assert(nodes[node]) end
local track = {seconds = 0, loop = false, curves = {}}
local config = {node = "face", keys = {'"Eye"'}, defaults = {0}, nodePath = '"Head/Face"',
    prefix = '{"schemaVersion":1,"morphs":[',
    facial = {["0"] = track}, faceBlend = .25, silent = track, voiced = track, speakIn = .1, speakOut = .25,
    look = {seconds = 1, eyes = {{1, .25}}, jointCurves = {{.15, .25, .35}}, targets = {{node = "face",
        prefix = '{"schemaVersion":2,"humanoidRotations":[', joints = {{index=1, prefix='{"path":"Head","offset":['}}}}},
    plank = {sampleRate = 2, frames = {{{0,0,0,1,0,0,0}}, {{0,0,0,1,.01,0,0}}},
        targets = {
            {node = "face", prefix = '{"schemaVersion":2', joints = {{index=1, prefix='{"path":"Head"'}}},
            {node = "body", prefix = '{"schemaVersion":2', joints = {{index=1, prefix='{"path":"Spine"'}}}
        }}
}
local face = new_story_face(host, config)
face:update(.25)
assert(nodes.body.pose:find('"translation":[0.005,0,0]', 1, true))
face:begin({category=18,commands={}}, false)
face:expression({look={scale=1,seconds=0,ease=0}})
face:update(.25)
assert(nodes.face.pose:find('"humanoidRotations":[{', 1, true))
assert(nodes.face.pose:find('"bones":[{', 1, true))
assert(nodes.face.pose:find('"targetId":"Eye","weight":0.25', 1, true))
face:finish({category=18}, false)
face:update(1.5)
assert(nodes.face.pose:find('"bones":[{', 1, true))
assert(not nodes.face.pose:find('"humanoidRotations"', 1, true))
assert(not nodes.face.pose:find('"morphs"', 1, true))
assert(nodes.body.pose:find('"translation":[0,0,0]', 1, true))
face:update(.75)
assert(nodes.body.pose:find('"translation":[0.005,0,0]', 1, true))
