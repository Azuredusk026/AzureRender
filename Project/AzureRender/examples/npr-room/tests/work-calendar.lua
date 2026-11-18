local state, date, failWrite, todo, action
local labels, visibility = {}, {}

self = {
    local_datetime = function() return date end,
    read_save = function(_, slot)
        if slot == "work-calendar" then return state end
        if slot == "todo" then return todo end
        assert(slot == "calendar-first-day")
    end,
    try_write_save = function(_, slot, proposed)
        assert(slot == "work-calendar")
        if failWrite then return false end
        state = proposed
        return true
    end,
    ui_text = function(_, id, value) labels[id] = value end,
    ui_visible = function(_, id, value) visibility[id] = value end,
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
