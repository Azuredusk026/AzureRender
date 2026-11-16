local asset, cursor, output = "", 0, false
local starts = {}
local paused = false
local action, saved
local display = {}
local saveAvailable = true

self = {}
function self:read_save(slot)
    assert(slot == "music")
    return saved
end
function self:try_write_save(slot, value)
    assert(slot == "music")
    if not saveAvailable then return false end
    saved = value
    return true
end
function self:set(component, field, value)
    assert(component == "azure.audio-source")
    if field == "asset" then asset = value end
end
function self:audio_play()
    cursor, output, paused = 0, true, false
    starts[#starts + 1] = asset
end
function self:audio_playing() return output and not paused end
function self:audio_position() return cursor end
function self:audio_pause(mode) paused = mode end
function self:audio_stop() output, paused = false, false end
function self:ui_text(id, text) display[id] = text end
function self:ui_visible() end
function self:pressed(name) return action == name end

local function click(name)
    action = "music-" .. name
    update()
    action = nil
end

local function finish()
    output, cursor = false, 100000
    update()
end

init()
assert(#starts == 1 and display["music-title"] == "Aqua Room")
output = false
update()
assert(#starts == 1, "A source waiting for its first mix must stay selected")
output = true
click("toggle")
click("repeat")
finish()
assert(#starts == 2 and starts[1] == starts[2])
click("repeat")
finish()
assert(#starts == 3 and starts[3] ~= starts[1] and display["music-title"] == "ChilDream")
cursor = 5
click("previous")
assert(#starts == 4 and starts[3] == starts[4])
click("previous")
assert(#starts == 5 and starts[5] == starts[1])
cursor = .9
click("play")
assert(paused and cursor == .9 and saved[2] == false)
click("quiet")
click("mute")
assert(#starts == 5 and paused and cursor == .9 and saved[3] == 45 and saved[4])
click("play")
assert(not paused and cursor == .9 and saved[2])
saveAvailable = false
click("shuffle")
assert(saved[5] == false and display["music-status"] ~= "")
saveAvailable = true
click("loud")
assert(saved[5] == true and display["music-status"] == "")

local anchor = asset
local encountered = {[anchor] = true}
for _ = 2, 42 do
    finish()
    assert(not encountered[asset])
    encountered[asset] = true
end
finish()
assert(asset == anchor)
