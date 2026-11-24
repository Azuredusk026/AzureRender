-- Exercise the production controller across independent script sessions.
local file = assert(io.open("examples/npr-room/assets/music.lua", "r"))
local source = file:read("*a")
file:close()
local config = [[local tracks = {
    {"one", "assets:/one.wav", "Original one", "Fixture", 2},
    {"two", "assets:/two.wav", "Original two", "Fixture", 2},
}
]]
local saved, writable = {1, true, 40, true, false}, true
local available = true
local function boot()
    local display, action, asset = {}, nil, nil
    local cursor, output = 0, false
    local host = {}
    function host:read_save() return saved end
    function host:try_write_save(_, value)
        if not writable then return false end
        saved = value
        return true
    end
    function host:set(_, field, value) if field == "asset" then asset = value end end
    function host:audio_play()
        if not available then error("Fixture audio unavailable") end
        cursor, output = 0, true
    end
    function host:try_audio_play()
        if not available then return false end
        self:audio_play()
        return true
    end
    function host:audio_stop() output = false end
    function host:audio_pause(value) output = not value end
    function host:audio_position() return cursor end
    function host:audio_playing() return output end
    function host:ui_text(id, text) display[id] = text end
    function host:ui_visible() end
    function host:pressed(name) return action == name end
    local env = setmetatable({self = host}, {__index = _G})
    assert(load(config .. source, "@production-music", "t", env))()
    env.init()
    env.update()
    local function click(name)
        action = "music-" .. name
        env.update()
        action = nil
    end
    local function finish()
        cursor, output = 2, false
        env.update()
        return asset
    end
    click("toggle")
    return click, finish, display
end

local click, finish, display = boot()
assert(display["music-volume"] == "40%" and display["music-mute"] == "Unmute")
click("repeat")
assert(saved[6] == true, "Repeat selection must be persisted immediately")
click, finish, display = boot()
assert(display["music-repeat"] == "Repeat one", "Reopen must restore repeat")
assert(finish() == "assets:/one.wav", "Restored repeat must replay the same track")
writable = false
click("repeat")
assert(saved[6] == true and display["music-status"] ~= "", "Failed write must preserve the live save")
writable = true
click("repeat")
click("repeat")
assert(saved[6] == false and display["music-status"] == "")
click, finish, display = boot()
assert(display["music-repeat"] == "Repeat off" and finish() == "assets:/two.wav")
saved = {1, false, 30, false, true, "invalid"}
click, finish, display = boot()
assert(display["music-play"] == "Play" and display["music-repeat"] == "Repeat off")
assert(display["music-volume"] == "30%" and display["music-shuffle"] == "Shuffle on")
print("Music repeat persistence, legacy preferences and failed-write recovery passed")

saved, available = {1, true, 45, true, false, true}, false
click, finish, display = boot()
assert(display["music-play"] == "Play" and display["music-status"] ~= "",
       "Unavailable autoplay must leave a visible, operable retry")
available = true
click("play")
assert(display["music-play"] == "Pause" and display["music-status"] == "")
assert(finish() == "assets:/one.wav", "Recovered playback must preserve the repeat preference")
print("Music unavailable autoplay and explicit recovery passed")

saved = {2, {"future preferences"}}
local protected = saved
click, finish, display = boot()
click("quiet")
assert(saved == protected and display["music-status"] ~= "", "Future music payload must remain intact")
saved = {1, false, 30, false, false}
click("loud")
assert(saved[1] == 1 and saved[3] == 50 and display["music-status"] == "")
saved = {2, {"newer external update"}}
protected = saved
click("mute")
assert(saved == protected, "A future payload installed after startup must remain intact")
print("Music payload version protection and same-session recovery passed")
