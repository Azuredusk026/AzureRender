local calls = {}
local actor = {}
function actor:set(component, field, value)
    assert(component == "azure.transform")
    calls[#calls + 1] = {field, value}
end
local host = {}
function host:find(node) assert(node == "source-camera"); return actor end
local track = {node = "source-camera", sampleRate = 2, translation = {1, 2, -3},
    rotations = {{179, 0, 0}, {-179, 2, 0}, {-175, 2, 0}}, finish = {178, 0, 0}}
local camera = new_story_camera(host, track)
camera:update(.25)
camera:effect({command = "DirectionMonochromeNoise"})
camera:finish()
assert(#calls == 0)
camera:effect({command = "DirectionGameStartCamera"})
assert(#calls == 2 and calls[1][1] == "translation" and calls[1][2][3] == -3)
assert(calls[2][2][1] == 179)
camera:update(.25)
assert(calls[3][2][1] == 180 and calls[3][2][2] == 1)
camera:update(.25)
assert(calls[4][2][1] == -179 and calls[4][2][2] == 2)
camera:update(3)
assert(calls[5][2][1] == -175)
camera:update(.1)
assert(#calls == 5)
camera:finish()
assert(#calls == 6 and calls[6][1] == "rotation" and calls[6][2][1] == 178)
camera:finish()
assert(#calls == 6)
camera:effect({command = "DirectionGameStartCamera"})
assert(#calls == 8 and calls[8][2][1] == 179)
local starts, stops = 0, 0
local available = true
local cue = {
    audio_play = function()
        if not available then error("Camera cue unavailable") end
        starts = starts + 1
    end,
    try_audio_play = function()
        if not available then return false end
        starts = starts + 1; return true
    end,
    audio_stop = function() stops = stops + 1 end,
}
local findCamera = host.find
function host:find(node)
    if node == "source-cue" then return cue end
    return findCamera(self, node)
end
track.cueNode = "source-cue"
local withSound = new_story_camera(host, track)
withSound:finish()
withSound:effect({command = "DirectionMonochromeNoise"})
assert(starts == 0 and stops == 0)
withSound:effect({command = "DirectionGameStartCamera"})
assert(starts == 1 and stops == 0)
withSound:update(3)
assert(starts == 1 and stops == 0)
withSound:finish()
assert(starts == 1 and stops == 1)
withSound:finish()
assert(stops == 1)

available = false
withSound:effect({command = "DirectionGameStartCamera"})
withSound:update(3); withSound:finish()
assert(calls[#calls][1] == "rotation" and calls[#calls][2][1] == 178)
available = true
withSound:effect({command = "DirectionGameStartCamera"})
assert(starts == 2, "A recovered optional camera cue must retry")
