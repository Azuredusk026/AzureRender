local action
local ui = {}
local sounds = {}
local saved
local saves = 0

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

function self:read_save(slot)
    assert(slot == "pomodoro-settings")
    return saved
end

function self:write_save(slot, value)
    assert(slot == "pomodoro-settings")
    saved = value
    saves = saves + 1
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

step(0, "timer-work-up")
step(0, "timer-break-up")
step(0, "timer-rounds-up")
expect("idle", "26:00")
assert(saves == 3 and saved[1] == 1 and saved[2] == 26 and saved[3] == 6 and saved[4] == 3)
step(0, "timer-toggle")
step(0, "timer-work-down")
expect("work", "26:00")
assert(saves == 3)
step(37)
expect("work", "25:23")
init()
expect("idle", "26:00")
assert(ui["timer-settings"] == "26m work · 6m break · 3 rounds")

saved = {1, 999, 1, 99}
init()
expect("idle", "999:00")
step(0, "timer-work-up")
step(0, "timer-break-down")
step(0, "timer-rounds-up")
assert(saves == 3)

saved = {2, 26, 0, 3}
init()
expect("idle", "25:00")
