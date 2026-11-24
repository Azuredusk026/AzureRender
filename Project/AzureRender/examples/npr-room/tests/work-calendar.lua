local state, date, failWrite, todo, action
local labels, visibility = {}, {}
local diaries, fields = {}, {}

self = {
    local_datetime = function() return date end,
    read_save = function(_, slot)
        if slot == "work-calendar" then return state end
        if slot == "todo" then return todo end
        if slot:match("^calendar%-diary%-") then return diaries[slot] end
        assert(slot == "calendar-first-day")
    end,
    try_write_save = function(_, slot, proposed)
        if failWrite then return false end
        if slot:match("^calendar%-diary%-") then
            diaries[slot] = proposed
        else
            assert(slot == "work-calendar")
            state = proposed
        end
        return true
    end,
    ui_text = function(_, id, value) labels[id] = value end,
    ui_visible = function(_, id, value) visibility[id] = value end,
    ui_value = function(_, id) return fields[id] end,
    ui_set_value = function(_, id, value) fields[id] = value end,
    pressed = function(_, id) return id == action end,
}

date = "20261231235958"
local calendar = new_work_calendar()
calendar:advance(2, true)
calendar:advance(5, false)
assert(calendar:timeFor("20261231") == 2)

date = "20270101000000"
calendar:advance(1.5, true)
assert(state[2][1][1] == "2026")
assert(calendar:timeFor("20261231") == 2 and calendar:timeFor("20270101") == 1.5)
assert(calendar:totalTime() == 3.5)

failWrite = true
assert(not calendar:flush())
assert(calendar:totalTime() == 3.5)
failWrite = false
assert(calendar:flush())
local reopened = new_work_calendar()
assert(reopened:timeFor("20261231") == 2 and reopened:timeFor("20270101") == 1.5)
assert(reopened:totalTime() == 3.5)
reopened:advance(60, true)
assert(new_work_calendar():totalTime() == 63.5)
reopened:advance(15, false)
assert(reopened:totalTime() == 63.5)

local second = new_work_calendar()
reopened:advance(0.25, true)
second:advance(0.75, true)
assert(reopened:flush() and second:flush())
assert(new_work_calendar():totalTime() == 64.5)

state = {2, {}}
local future = new_work_calendar()
future:advance(1, true)
assert(not future:flush() and state[1] == 2 and future:timeFor("20270101") == 1)

state = {1, {{"2027", {{"0101", 10}, {"0102", 40}}}}}
local editor = new_work_calendar()
editor:advance(3, true)
failWrite = true
assert(not editor:setTimeFor("20270102", 3661))
assert(editor:timeFor("20270101") == 13 and editor:timeFor("20270102") == 40)
failWrite = false
assert(editor:setTimeFor("20270102", 3661))
assert(editor:timeFor("20270101") == 13 and editor:timeFor("20270102") == 3661)
assert(editor:setTimeFor("20270102", 0))
assert(not editor:setTimeFor("20270229", 24))
assert(not editor:setTimeFor("20270102", 86401))
assert(not editor:setTimeFor("20270102", 1.25))
assert(new_work_calendar():totalTime() == 13)
state = {2, {}}
assert(not new_work_calendar():setTimeFor("20270101", 4) and state[1] == 2)

state = {1, {{"2027", {{"0101", 10.25}}}}}
local liveEditor = new_work_calendar()
local savedBeforeBlur = state
action = "calendar-edit-time"
liveEditor:update_month_view(false)
assert(fields["calendar-edit-seconds"] == "10")
liveEditor:advance(2, true)
action = "calendar-submit-time"
liveEditor:update_month_view(false)
assert(state == savedBeforeBlur and liveEditor:timeFor("20270101") == 12.25,
    "Leaving unchanged work time fields must preserve the running timer's work")
assert(fields["calendar-edit-seconds"] == "12")
assert(liveEditor:flush() and new_work_calendar():timeFor("20270101") == 12.25)
fields["calendar-edit-seconds"] = "20"
liveEditor:update_month_view(false)
assert(new_work_calendar():timeFor("20270101") == 20)

fields["calendar-edit-seconds"] = "32"
failWrite = true
assert(not liveEditor:close_month_view(), "A rejected work edit must keep the calendar open")
assert(visibility["calendar-work-time-editor"] and fields["calendar-edit-seconds"] == "32")
assert(labels["calendar-status"] == "Could not save work time" and liveEditor:timeFor("20270101") == 20)
action = "calendar-edit-time"
liveEditor:update_month_view(false)
assert(visibility["calendar-work-time-editor"] and fields["calendar-edit-seconds"] == "32")
date = "20270102000000"
action = nil
assert(liveEditor:update_month_view(false) == "2027-01-01")
action = "calendar-day-7"
local selected = liveEditor:update_month_view(false)
assert(selected == "2027-01-01" and fields["calendar-edit-seconds"] == "32",
    "A rejected work edit must keep its original date and draft")
failWrite = false
selected = liveEditor:update_month_view(false)
assert(selected == "2027-01-02" and not visibility["calendar-work-time-editor"])
assert(new_work_calendar():timeFor("20270101") == 32)
date = "20270101000000"
action = nil

todo = {1, 6, {
    {1, "Daily", {{2, "Second", "20270101091200", ""}, {3, "Future", "20270102063700", "2027-01-01"}}},
    {4, "Notes", {{5, "Earlier", "20270101084305", ""}}},
}}
local history = new_work_calendar()
history:update_month_view(false)
assert(visibility["calendar-completions"])
assert(labels["calendar-completed-tasks"] == "08:43:05 Earlier\n09:12:00 Second")
action = "todo-task-complete"
history:update_month_view(false)
todo[3][2][3][1][3] = ""
action = nil
history:update_month_view(false)
assert(labels["calendar-completed-tasks"] == "09:12:00 Second")
action = "calendar-day-7"
history:update_month_view(false)
assert(labels["calendar-completed-tasks"] == "06:37:00 Future")
history:close_month_view()
todo = {2, 1, {}}
action = nil
history:update_month_view(false)
assert(not visibility["calendar-completions"] and labels["calendar-completed-tasks"] == "")

local diary = new_work_calendar()
action = "calendar-diary-toggle"
assert(diary:update_month_view(false) == "2027-01-01")
assert(visibility["calendar-diary-editor"])
fields["calendar-diary-text"] = "日记\nSecond <line>"
failWrite = true
action = "calendar-day-7"
assert(diary:update_month_view(false) == "2027-01-01")
assert(fields["calendar-diary-text"] == "日记\nSecond <line>")
assert(not diary:close_month_view())
failWrite = false
assert(diary:update_month_view(false) == "2027-01-02")
assert(diaries["calendar-diary-20270101"][2] == "日记\nSecond <line>")
assert(fields["calendar-diary-text"] == "")
diaries["calendar-diary-20270102"] = {2, "Protected"}
fields["calendar-diary-text"] = "Draft"
assert(not diary:close_month_view() and diaries["calendar-diary-20270102"][2] == "Protected")
action = "calendar-diary-cancel"
diary:update_month_view(false)
assert(diary:close_month_view() and not visibility["calendar-diary-editor"])
action = "calendar-day-6"
diary:update_month_view(false)
action = "calendar-diary-toggle"
diary:update_month_view(false)
fields["calendar-diary-text"] = "Before closing"
action = nil
diary:update_month_view(false)
assert(diary:save_open_diary())
assert(diaries["calendar-diary-20270101"][2] == "Before closing")
assert(diaries["calendar-diary-20270102"][2] == "Protected")
fields["calendar-diary-text"] = string.rep("文", 4999) .. "🌙"
diary:update_month_view(false)
assert(not diary:save_open_diary())
assert(diaries["calendar-diary-20270101"][2] == "Before closing")
fields["calendar-diary-text"] = string.rep("文", 4998) .. "🌙"
diary:update_month_view(false)
assert(diary:save_open_diary())
