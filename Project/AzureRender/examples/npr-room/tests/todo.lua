local action, stored, failed
local values, labels, visible = {}, {}, {}
local sounds = {}
local audioAvailable = true
local when = "20261117024018"

self = {
    pressed = function(_, name) return name == action end,
    find = function(_, id)
        return {
            audio_play = function()
                if not audioAvailable then error("Task cue unavailable") end
                sounds[#sounds + 1] = id
            end,
            try_audio_play = function()
                if not audioAvailable then return false end
                sounds[#sounds + 1] = id
                return true
            end,
        }
    end,
    local_datetime = function() return when end,
    read_save = function(_, slot) assert(slot == "todo"); return stored end,
    try_write_save = function(_, slot, payload)
        assert(slot == "todo")
        if failed then return false end
        stored = payload
        return true
    end,
    ui_value = function(_, id) return values[id] or "" end,
    ui_set_value = function(_, id, value) values[id] = value end,
    ui_text = function(_, id, value) labels[id] = value end,
    ui_visible = function(_, id, value) visible[id] = value end,
}

local function press(name)
    action = "todo-" .. name
    update(1 / 60)
    action = nil
end

init()
press("toggle")
assert(visible["todo-panel"] and not visible["todo-task-details"])
values["todo-list-title"] = "项目"
press("task-new")
values["todo-task-text"] = "设计"
values["todo-task-due"] = "2028-02-29"
press("task-new")
values["todo-task-text"] = "验收"
values["todo-task-due"] = "2027-02-29"
press("save")
assert(stored[3][1][3][2][2] == "" and labels["todo-status"] ~= "")
values["todo-task-due"] = ""
press("task-up")
assert(stored[3][1][3][1][1] == 3 and stored[3][1][3][2][1] == 2)
press("task-complete")
assert(stored[3][1][3][1][3] == when and labels["todo-progress"] == "1 / 2")
assert(sounds[#sounds] == "todo-complete")
press("list-new")
values["todo-list-title"] = "日程"
press("task-new")
values["todo-task-text"] = "会议"
press("list-prev")
assert(values["todo-list-title"] == "项目" and values["todo-task-text"] == "设计")
press("task-next")
assert(values["todo-task-text"] == "验收")
press("task-complete")
assert(stored[3][1][3][1][3] == "" and labels["todo-progress"] == "0 / 2")
assert(sounds[#sounds] == "settings-cancel")
press("list-next")
assert(values["todo-list-title"] == "日程" and values["todo-task-text"] == "会议")
press("list-delete")
press("list-delete")
assert(#stored[3] == 1 and stored[3][1][1] == 1)

init()
press("toggle")
assert(values["todo-task-text"] == "验收")
press("task-next")
assert(values["todo-task-text"] == "设计" and values["todo-task-due"] == "2028-02-29")
values["todo-task-text"] = "改进"
failed = true
press("save")
assert(stored[3][1][3][2][2] == "设计" and values["todo-task-text"] == "改进")
failed = false
press("save")
assert(stored[3][1][3][2][2] == "改进")
press("task-delete")
assert(#stored[3][1][3] == 1)

audioAvailable = false
press("task-complete")
assert(stored[3][1][3][1][3] == when and labels["todo-progress"] == "1 / 1")
press("task-complete")
assert(stored[3][1][3][1][3] == "" and labels["todo-progress"] == "0 / 1")
audioAvailable = true
press("task-complete")
assert(stored[3][1][3][1][3] == when and sounds[#sounds] == "todo-complete")

local future = {2, "future tasks"}
stored = future
press("task-complete")
assert(stored == future, "Future task payload installed during the session must remain intact")
stored = nil
press("task-complete")
assert(stored[1] == 1 and stored[3][1][3][1][3] == "")
future = {3, "future tasks at startup"}
stored = future
init()
press("toggle")
press("task-new")
assert(stored == future)
stored = nil
press("task-new")
assert(stored[1] == 1 and #stored[3][1][3] == 1, "Task writes must recover in the same session")

values["todo-list-title"] = "Exit list"
values["todo-task-text"] = "Unsubmitted draft"
values["todo-task-due"] = "2028-02-29"
assert(type(shutdown) == "function", "An open task draft needs a shutdown handler")
shutdown()
assert(stored[3][1][2] == "Exit list" and stored[3][1][3][1][2] == "Unsubmitted draft")
assert(stored[3][1][3][1][4] == "2028-02-29")

values["todo-task-text"] = "Invalid date draft"
values["todo-task-due"] = "2027-02-29"
local committed = stored
shutdown()
assert(stored == committed and labels["todo-status"] ~= "", "Exit must validate the draft")
values["todo-task-due"] = ""
failed = true
shutdown()
assert(stored == committed and values["todo-task-text"] == "Invalid date draft")
failed = false
stored = {2, "future tasks on exit"}
local protected = stored
shutdown()
assert(stored == protected, "Exit must preserve a newer task payload")
stored = committed
shutdown()
assert(stored[3][1][3][1][2] == "Invalid date draft", "Exit retries the retained draft")
press("toggle")
committed = stored
shutdown()
assert(stored == committed, "A closed panel has no pending draft")
