local actions, display, visible, sources, saved = {}, {}, {}, {}, nil
self = {}
function self:pressed(action) return actions[action] end
function self:read_save(slot) assert(slot == "ambient"); return saved end
function self:try_write_save(slot, value) assert(slot == "ambient"); saved = value; return true end
function self:ui_text(id, value) display[id] = value end
function self:ui_visible(id, value) visible[id] = value end
function self:find(id)
    if not sources[id] then
        local source = {starts = 0}
        function source:set(component, key, value) assert(component == "azure.audio-source"); self[key] = value end
        function source:audio_play() self.starts = self.starts + 1 end
        sources[id] = source
    end
    return sources[id]
end

local function click(action)
    actions["ambient-" .. action] = true
    update()
    actions = {}
end

init()
assert(not visible["ambient-panel"] and next(sources) == nil)
click("toggle")
assert(display["ambient-page"] == "1 / 4" and visible["ambient-select-8"] and not visible["ambient-select-9"])
click("select-8")
click("mute")
local rain = sources["room-ambient-8"]
assert(rain.volume == .5 and rain.starts == 1)
click("quiet")
click("mute")
assert(rain.volume == 0 and rain.starts == 1 and saved[2][1][2] == 45)
click("mute")
assert(rain.volume == .45 and rain.starts == 1)
click("page-next")
assert(display["ambient-page"] == "2 / 4" and not visible["ambient-select-8"] and visible["ambient-select-9"])
click("select-9")
click("loud")
local heavy = sources["room-ambient-9"]
assert(heavy.volume == .05 and heavy.starts == 1 and rain.volume == .45)
