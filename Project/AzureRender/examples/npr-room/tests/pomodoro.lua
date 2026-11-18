local action
local ui = {}
local sounds = {}
local saved
local saves = 0
local otherSaves = {}
local failedSlot
local bodyState

self = {}
function self:pressed(name)
    return action == name
end

function self:ui_text(id, text)
    ui[id] = text
end

function self:find(id)
    if id == "heroine-body" then
        return { set = function(_, component, field, state)
            assert(component == "azure.animator" and field == "state")
            bodyState = state
        end }
    end
    if id:sub(1, 8) == "heroine-" then return nil end
    return { audio_play = function() sounds[#sounds + 1] = id end }
end

function self:read_save(slot)
    if slot == "pomodoro-settings" then return saved end
    return otherSaves[slot]
end

function self:write_save(slot, value)
    assert(self:try_write_save(slot, value), "save failure")
end

function self:try_write_save(slot, value)
    if slot == failedSlot then return false end
    if slot == "pomodoro-settings" then
        saved = value
        saves = saves + 1
    else
        otherSaves[slot] = value
    end
    return true
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
assert(bodyState == "report")
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
assert(bodyState == "idle")
assert(sounds[6] == "timer-finish")
step(0, "timer-toggle")
step(2)
step(0, "timer-reset")
expect("idle", "25:00")
assert(sounds[8] == "timer-finish")

failedSlot = "pomodoro-settings"
local soundsBeforeFailure = #sounds
step(0, "timer-work-up")
expect("idle", "25:00")
assert(saves == 0 and #sounds == soundsBeforeFailure and saved == nil)
failedSlot = nil

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

failedSlot = "timer-mode"
soundsBeforeFailure = #sounds
step(0, "timer-mode")
expect("idle", "25:00")
assert(otherSaves["timer-mode"] == nil and #sounds == soundsBeforeFailure)
failedSlot = nil

step(0, "timer-mode")
expect("idle", "00:00:00")
assert(otherSaves["timer-mode"][2] == 1)
step(0, "timer-toggle")
step(2)
expect("work", "00:00:02")
step(0, "timer-toggle")
assert(sounds[#sounds] == "timer-finish")
step(1, "timer-mode")
expect("paused", "00:00:02")
step(12)
expect("paused", "00:00:02")
step(0, "timer-toggle")
assert(sounds[#sounds] == "timer-start")
step(58)
expect("work", "00:01:00")
assert(otherSaves["countup-session"] == nil)
step(60)
assert(otherSaves["countup-session"][2] == 120)
step(0, "timer-reset")
expect("idle", "00:00:00")
assert(otherSaves["countup-session"][2] == 0 and otherSaves["countup-session"][3] == 120)

step(0, "timer-toggle")
step(7)
shutdown()
assert(otherSaves["countup-session"][2] == 7)
init()
expect("idle", "00:00:00")
step(0)
assert(otherSaves["countup-session"][2] == 0 and otherSaves["countup-session"][3] == 7)
init()
assert(otherSaves["countup-session"][3] == 7)

step(0, "timer-toggle")
step(359999)
expect("work", "99:59:59")
step(1)
expect("idle", "00:00:00")
assert(otherSaves["countup-session"][3] == 359999)
step(0, "timer-mode")
expect("idle", "25:00")
assert(otherSaves["timer-mode"][2] == 0)

step(0, "timer-mode")
step(0, "timer-toggle")
failedSlot = "countup-session"
step(60)
expect("work", "00:01:00")
assert(otherSaves["countup-session"][2] == 0)
step(2)
step(0, "timer-reset")
expect("idle", "00:00:00")
assert(otherSaves["countup-session"][3] == 359999)
failedSlot = nil
step(0.5)
assert(otherSaves["countup-session"][3] == 359999)
step(0.5)
assert(otherSaves["countup-session"][2] == 0 and otherSaves["countup-session"][3] == 62)

otherSaves["countup-session"] = {1, 7, 62}
failedSlot = "countup-session"
init()
step(0)
expect("idle", "00:00:00")
assert(otherSaves["countup-session"][2] == 7)
failedSlot = nil
shutdown()
assert(otherSaves["countup-session"][2] == 0 and otherSaves["countup-session"][3] == 7)
