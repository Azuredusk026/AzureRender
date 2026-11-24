local preferences = {mode = 0, resolution = 3, alwaysOnTop = false, pending = false}
local saved, failure, action, frameRates, sync
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
    if slot == "room-vsync" then return sync end
    assert(slot == "room-window-settings")
    return saved
end
function self:try_write_save(slot, value)
    assert(slot == "room-window-settings" or slot == "room-frame-rates" or slot == "room-vsync")
    if failure then return false end
    if slot == "room-frame-rates" then
        frameRates = value
    elseif slot == "room-vsync" then
        sync = value
    else
        saved = value
    end
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

preferences.vsync = false
press("display-toggle")
assert(text["display-vsync"] == "Off")
press("display-vsync")
assert(preferences.vsync and sync[2] == true and text["display-vsync"] == "On")
failure = true
press("display-vsync")
assert(preferences.vsync and sync[2] == true)
failure = false
preferences.vsync = false
init()
assert(preferences.vsync and not visible["display-panel"])
sync = {2, false}
init()
press("display-toggle")
press("display-vsync")
assert(preferences.vsync and sync[1] == 2)

sync = nil
press("display-vsync")
assert(sync and sync[1] == 1 and not preferences.vsync, "Vsync writes must recover without restarting")
local futureSync = {3, true}
sync = futureSync
press("display-vsync")
assert(sync == futureSync and not preferences.vsync, "Runtime future vsync data must stay intact")
preferences.pending = false
local futureWindow = {3, 0, 3, false}
saved = futureWindow
local topmost = preferences.alwaysOnTop
press("display-topmost")
assert(saved == futureWindow and preferences.alwaysOnTop == topmost)
saved = nil
press("display-topmost")
assert(saved[1] == 1 and preferences.alwaysOnTop ~= topmost)
preferences.pending = false
local futureRates = {3, 60, 24}
frameRates = futureRates
local rate = preferences.activeFrameRate
fields["display-frame-active"] = "120"
press("display-frame-active")
assert(frameRates == futureRates and preferences.activeFrameRate == rate)
frameRates = nil
fields["display-frame-active"] = "120"
press("display-frame-active")
assert(frameRates[1] == 1 and preferences.activeFrameRate == 120)
