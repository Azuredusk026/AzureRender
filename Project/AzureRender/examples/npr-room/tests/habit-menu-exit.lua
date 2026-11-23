local stored, action, blocked
local values, labels, visible = {}, {}, {}
local writes = 0
local function copy(value)
    if type(value) ~= "table" then return value end
    local result = {}
    for key, item in pairs(value) do result[key] = copy(item) end
    return result
end
self = {
    read_save = function(_, slot) if slot == "habit-tracker" then return copy(stored) end end,
    try_write_save = function(_, slot, value)
        if slot ~= "habit-tracker" then return true end
        writes = writes + 1
        if blocked then return false end
        stored = copy(value)
        return true
    end,
    pressed = function(_, id) return action == id end,
    find = function() return nil end,
    local_datetime = function() return "20261124093000" end,
    ui_set_value = function(_, id, value) values[id] = value end,
    ui_value = function(_, id) return values[id] or "" end,
    ui_text = function(_, id, value) labels[id] = value end,
    ui_visible = function(_, id, value) visible[id] = value end,
}
local function press(name)
    action = name
    update(1 / 60)
    action = nil
end
init()
press("timer-stats")
press("calendar-habits-toggle")
press("calendar-habit-new")
values["calendar-habit-title"] = "Exit draft"
update(1 / 60)
shutdown()
assert(stored[3][1][2] == "Exit draft", "An unblurred habit name must survive Player exit")
local committedWrites = writes
shutdown()
assert(writes == committedWrites, "Unchanged habit names do not need another commit")

values["calendar-habit-title"] = "Retained after failure"
update(1 / 60)
blocked = true
shutdown()
assert(stored[3][1][2] == "Exit draft" and values["calendar-habit-title"] == "Retained after failure")
blocked = false
local previous = stored
stored = {2, "future habits"}
shutdown()
assert(stored[1] == 2)
stored = previous
shutdown()
assert(stored[3][1][2] == "Retained after failure")

values["calendar-habit-title"] = "Calendar close draft"
blocked = true
press("timer-stats")
assert(visible["work-stats"] and values["calendar-habit-title"] == "Calendar close draft")
assert(stored[3][1][2] == "Retained after failure")
blocked = false
press("timer-stats")
assert(stored[3][1][2] == "Calendar close draft" and not visible["work-stats"])
init()
press("timer-stats")
press("calendar-habits-toggle")
assert(values["calendar-habit-title"] == "Calendar close draft")
values["calendar-habit-title"] = "Habit panel close draft"
press("calendar-habits-toggle")
assert(stored[3][1][2] == "Habit panel close draft" and not visible["calendar-habit-editor"])
