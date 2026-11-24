local saves, values, visible = {}, {}, {}
local action, failedSlot
self = {}
function self:read_save(slot) return saves[slot] end
function self:try_write_save(slot, value)
    if slot == failedSlot then return false end
    saves[slot] = value
    return true
end
function self:pressed(name) return name == action end
function self:ui_value(id) return values[id] or "" end
function self:ui_set_value(id, value) values[id] = value end
function self:ui_visible(id, value) visible[id] = value end
function self:ui_text() end
function self:find() end
function self:local_datetime() return "20261118090000" end

local function press(name)
    action = name
    update(0)
    action = nil
end

init()
local slot, commit, key
if visible["notes-toggle"] then
    slot, commit, key = "notes", "notes-save", "END_EDIT_NOTE_COUNT"
    press("notes-toggle")
    values["note-title"] = "Renamed"
    press(commit)
    assert(saves[slot][4] == nil and saves["room-achievements"] == nil)
    values["note-text"] = "Saved note"
else
    slot, commit, key = "todo", "todo-task-complete", "COMPLETE_TODO_COUNT"
    press("todo-toggle")
    press("todo-task-new")
end
failedSlot = slot
press(commit)
assert(saves[slot][4] == nil and saves["room-achievements"] == nil)
failedSlot = "room-achievements"
press(commit)
assert(saves[slot][4] == true and saves["room-achievements"] == nil)
failedSlot = nil
init()
assert(saves["room-achievements"][2][1] == key)
init()
assert(#saves["room-achievements"][2] == 1)
