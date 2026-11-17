local preferences = {mode = 0, resolution = 3, alwaysOnTop = false, pending = false}
local saved, failure, action, frameRates
local text, visible, fields = {}, {}, {}
local window = {}
function window:get(component, field)
    assert(component == "azure.window-presentation")
    return preferences[field]
end
function window:set(component, field, value)
    assert(component == "azure.window-presentation")
    preferences[field] = value
end
self = {}
function self:find(name) assert(name == "room-display-window"); return window end
function self:read_save(slot)
    if slot == "room-frame-rates" then return frameRates end
    assert(slot == "room-window-settings")
    return saved
end
function self:try_write_save(slot, value)
    assert(slot == "room-window-settings" or slot == "room-frame-rates")
    if failure then return false end
    if slot == "room-frame-rates" then frameRates = value else saved = value end
    return true
end
function self:pressed(name) return action == name end
function self:ui_visible(id, state) visible[id] = state end
function self:ui_text(id, value) text[id] = value end
function self:ui_value(id) return fields[id] end
function self:ui_set_value(id, value) fields[id] = value end

local function press(id)
    action = id
    update()
    action = nil
end

init()
press("display-toggle")
assert(visible["display-panel"] and visible["display-resolution-row"])
assert(text["display-resolution"] == "Custom" and saved == nil)
press("display-mode")
assert(preferences.mode == 1 and preferences.pending and not visible["display-resolution-row"])
assert(saved[2] == 1 and saved[3] == 3 and saved[4] == false)
preferences.pending = false
press("display-topmost")
assert(preferences.alwaysOnTop and saved[4])
preferences.pending = false
press("display-mode")
press("display-resolution")
assert(preferences.mode == 0 and preferences.resolution == 0 and preferences.pending)
preferences.pending = false
preferences.resolution = 3
update()
assert(saved[3] == 3 and not preferences.pending and text["display-resolution"] == "Custom")
failure = true
press("display-resolution")
assert(not preferences.pending and preferences.resolution == 3 and text["display-status"] ~= "")
failure = false
press("display-resolution")
assert(preferences.resolution == 0 and text["display-status"] == "")
preferences = {mode = 0, resolution = 3, alwaysOnTop = false, pending = false}
init()
assert(preferences.resolution == 0 and preferences.alwaysOnTop and preferences.pending)

preferences = {mode = 0, resolution = 3, alwaysOnTop = false, pending = false}
local later = {2, 0, 3, false}
saved = later
init()
press("display-toggle")
press("display-topmost")
assert(saved == later and not preferences.pending and not preferences.alwaysOnTop)

saved = nil
init()
press("display-topmost-menu")
assert(not visible["display-panel"] and preferences.alwaysOnTop and preferences.pending and saved[4])
preferences.pending = false
failure = true
press("display-topmost-menu")
assert(not preferences.pending and preferences.alwaysOnTop and saved[4])

failure = false
preferences.activeFrameRate, preferences.inactiveFrameRate = 60.0, 24.0
init()
press("display-toggle")
assert(fields["display-frame-active"] == "60" and fields["display-frame-inactive"] == "24")
fields["display-frame-active"] = "256"
press("display-frame-active")
assert(preferences.activeFrameRate == 250 and fields["display-frame-active"] == "250")
fields["display-frame-inactive"] = "1"
press("display-frame-inactive")
assert(preferences.inactiveFrameRate == 10 and frameRates[2] == 250 and frameRates[3] == 10)
fields["display-frame-active"] = "62.5"
press("display-frame-active")
assert(preferences.activeFrameRate == 250 and fields["display-frame-active"] == "250")
failure = true
fields["display-frame-inactive"] = "64"
press("display-frame-inactive")
assert(preferences.inactiveFrameRate == 10 and fields["display-frame-inactive"] == "10")
failure = false
preferences.activeFrameRate, preferences.inactiveFrameRate = 60, 24
init()
assert(preferences.activeFrameRate == 250 and preferences.inactiveFrameRate == 10)
