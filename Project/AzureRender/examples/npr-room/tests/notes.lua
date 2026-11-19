local action
local stored
local values = {}
local labels = {}
local displayed = {}
local failWrite = false

self = {
    pressed = function(_, name) return action == name end,
    read_save = function(_, slot) assert(slot == "notes"); return stored end,
    try_write_save = function(_, slot, value)
        assert(slot == "notes")
        if failWrite then return false end
        stored = value
        return true
    end,
    ui_value = function(_, id) return values[id] or "" end,
    ui_set_value = function(_, id, value) values[id] = value end,
    ui_text = function(_, id, value) labels[id] = value end,
    ui_visible = function(_, id, visible) displayed[id] = visible end,
}

local function press(name)
    action = name
    update(1 / 60)
    action = nil
end

init()
assert(displayed["notes-panel"] == false)
press("notes-toggle")
assert(displayed["notes-panel"] == true and displayed["note-fields"] == true)
assert(values["note-title"] == "New Page")
values["note-title"] = "原始笔记"
values["note-text"] = "第一行\n<笔记>"
failWrite = true
press("notes-save")
assert(stored == nil and labels["note-status"] == "Could not save this note.")
assert(values["note-text"] == "第一行\n<笔记>")
failWrite = false
press("notes-save")
assert(labels["note-status"] == "")
assert(stored[3][1][1] == 1 and stored[3][1][2] == "原始笔记" and stored[3][1][3] == "第一行\n<笔记>")
press("notes-new")
values["note-text"] = "second page"
press("notes-up")
assert(stored[3][1][1] == 2 and stored[3][2][1] == 1)
assert(values["note-text"] == "second page")
press("notes-next")
assert(values["note-text"] == "第一行\n<笔记>")
press("notes-delete")
assert(#stored[3] == 1 and stored[3][1][1] == 2)
assert(values["note-text"] == "second page")
press("notes-delete")
assert(#stored[3] == 1)

init()
press("notes-toggle")
assert(values["note-text"] == "second page")
values["note-text"] = "edited on close"
press("notes-toggle")
assert(stored[3][1][3] == "edited on close")
press("notes-toggle")
press("notes-new")
assert(stored[3][2][1] == 3)

stored = {1, 3, {{2, "first", ""}, {2, "invalid duplicate", ""}}}
init()
press("notes-toggle")
assert(values["note-title"] == "New Page" and values["note-text"] == "")

stored = {1, 2, {{1, "Notebook", "saved text"}}}
init()
press("notes-toggle")
values["note-text"] = "saved on Player exit"
assert(type(shutdown) == "function", "notes must publish an open draft on Player exit")
shutdown()
assert(stored[3][1][3] == "saved on Player exit")

init()
press("notes-toggle")
values["note-text"] = "unsaved replacement"
failWrite = true
shutdown()
assert(stored[3][1][3] == "saved on Player exit")
assert(values["note-text"] == "unsaved replacement")
assert(labels["note-status"] == "Could not save this note.")
failWrite = false

local future = {2, "preserve"}
stored = future
init()
press("notes-toggle")
values["note-text"] = "future version replacement"
shutdown()
assert(stored == future)
