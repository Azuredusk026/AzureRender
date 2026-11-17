local saves, action, failed = {}, nil, false
self = {}
function self:read_save(key) return saves[key] end
function self:try_write_save(key, value)
    if failed and key == "room-achievements" then return false end
    saves[key] = value
    return true
end
function self:write_save(key, value) assert(self:try_write_save(key, value)) end
function self:local_datetime() return "20261118090000" end
function self:pressed(key) return action == key end
function self:find() end
function self:ui_text() end
function self:ui_visible() end

local function press(key)
    action = key
    update(0)
    action = nil
end

init()
assert(saves["room-achievements"] == nil)
press("timer-mode")
press("timer-toggle")
update(1)
failed = true
press("timer-reset")
assert(saves["work-calendar"][3] == true)
assert(saves["room-achievements"] == nil)
failed = false
init()
assert(saves["room-achievements"][2][1] == "COMPLETE_POMODORO_COUNT")
press("timer-mode")
press("timer-toggle")
press("timer-reset")
assert(#saves["room-achievements"][2] == 1)
