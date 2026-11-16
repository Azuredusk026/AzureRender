local action
local ui = {}
local sounds = {}

self = {}
function self:pressed(name)
    return action == name
end

function self:ui_text(id, text)
    ui[id] = text
end

function self:find(id)
    return { audio_play = function() sounds[#sounds + 1] = id end }
end

local function step(seconds, input)
    action = input
    update(seconds)
    action = nil
end

local function expect(phase, timer)
    assert(ui["timer-phase"] == phase, "unexpected phase: " .. tostring(ui["timer-phase"]))
    assert(ui["timer-value"] == timer, "unexpected elapsed display: " .. tostring(ui["timer-value"]))
end

init()
expect("idle", "25:00")
step(0, "timer-toggle")
expect("work", "25:00")
assert(sounds[1] == "timer-start")
step(1)
expect("work", "24:59")
step(0, "timer-toggle")
expect("paused", "24:59")
assert(sounds[2] == "timer-pause")
step(5)
expect("paused", "24:59")
step(0, "timer-toggle")
expect("work", "24:59")
assert(sounds[3] == "timer-start")
step(1499)
expect("break", "05:00")
assert(sounds[4] == "timer-finish")
step(300)
expect("work", "25:00")
assert(sounds[5] == "timer-start")
step(0, "timer-skip")
expect("idle", "25:00")
assert(sounds[6] == "timer-finish")
step(0, "timer-toggle")
step(2)
step(0, "timer-reset")
expect("idle", "25:00")
assert(sounds[8] == "timer-finish")
