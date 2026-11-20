local file = assert(io.open("examples/npr-room/assets/ambient.lua", "r"))
local script = file:read("*a")
file:close()
local saved = {1, {{1, 45, false}, {2, 30, false}}}
local writable, available, action = true, false, nil
local sources, display = {}, {}
local host = {}
function host:read_save() return saved end
function host:try_write_save(_, value)
    if not writable then return false end
    saved = value
    return true
end
function host:find(id)
    if not sources[id] then
        local source = {starts = 0}
        function source:set(_, field, value) self[field] = value end
        function source:audio_play()
            if id == "first" and not available then error("Audio unavailable") end
            self.starts = self.starts + 1
        end
        function source:try_audio_play()
            if id == "first" and not available then return false end
            self:audio_play()
            return true
        end
        sources[id] = source
    end
    return sources[id]
end
function host:ui_text(id, text) display[id] = text end
function host:ui_visible() end
function host:pressed(name) return name == action end
local config = 'local sounds = {{1,"first","assets:/one.wav","Original one"}, {2,"second","assets:/two.wav","Original two"}}\n'
local env = setmetatable({self = host}, {__index = _G})
assert(load(config .. script, "@production-ambient", "t", env))()
env.init()
env.update()
assert(sources.first.starts == 0 and sources.second.starts == 1)
local function click(name)
    action = "ambient-" .. name
    env.update()
    action = nil
end
click("toggle")
assert(display["ambient-status"] ~= "", "Unavailable restored ambience must remain operable")
available = true
click("loud")
assert(sources.first.starts == 1 and sources.first.volume == .5 and sources.second.starts == 1)
assert(display["ambient-status"] == "" and saved[2][1][2] == 50)
writable = false
click("quiet")
assert(display["ambient-status"] ~= "" and saved[2][1][2] == 50)
writable = true
click("loud")
assert(display["ambient-status"] == "" and sources.first.starts == 1)
print("Ambient restored failure, independent source, explicit recovery and failed-write retry passed")

saved = {2, {"future ambient preferences"}}
local protected = saved
click("loud")
assert(saved == protected and display["ambient-status"] ~= "", "Future ambient payload must remain intact")
saved = {1, {{1, 45, false}}}
click("quiet")
assert(saved[1] == 1 and saved[2][1][2] == 50 and display["ambient-status"] == "")
print("Ambient payload version protection and same-session recovery passed")
