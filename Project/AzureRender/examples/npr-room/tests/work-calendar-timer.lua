local pressed, wall
local saved = {}
local labels = {}

self = {
    find = function() return nil end,
    pressed = function(_, name) return name == pressed end,
    local_datetime = function() return wall end,
    read_save = function(_, slot) return saved[slot] end,
    write_save = function(_, slot, value) saved[slot] = value end,
    try_write_save = function(_, slot, value) saved[slot] = value; return true end,
    ui_text = function(_, id, text) labels[id] = text end,
    ui_visible = function() end,
}

local function step(dt, key)
    pressed = key
    update(dt)
    pressed = nil
end

wall = "20261117235957"
init()
step(0, "timer-toggle")
step(3)
wall = "20261118000000"
step(2)
step(0, "timer-toggle")
step(10)
step(0, "timer-toggle")
step(0, "timer-skip")
step(11)
step(0, "timer-reset")
local days = saved["work-calendar"][2][1][2]
assert(days[1][1] == "1117" and days[1][2] == 3)
assert(days[2][1] == "1118" and days[2][2] == 2)
step(0, "timer-stats")
assert(labels["work-today"] == "00:00:02" and labels["work-total"] == "00:00:05")

step(0, "timer-mode")
step(0, "timer-toggle")
step(2)
shutdown()
init()
step(0, "timer-stats")
assert(labels["work-today"] == "00:00:04" and labels["work-total"] == "00:00:07")
assert(new_work_calendar():totalTime() == 7)

local weekSlot = "calendar-first-day"
local rejectWeek = false
self.try_write_save = function(_, slot, value)
    if slot == weekSlot and rejectWeek then return false end
    saved[slot] = value
    return true
end

wall = "20240229120000"
init()
step(0, "timer-stats")
assert(labels["calendar-month"] == "2024-02")
assert(labels["calendar-weekday-1"] == "Sun" and labels["calendar-weekday-7"] == "Sat")
assert(labels["calendar-date-4"] == "" and labels["calendar-date-5"] == "1")
assert(labels["calendar-date-33"] == "29" and labels["calendar-date-34"] == "")

local weekdays = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"}
for first = 0, 6 do
    step(0, "calendar-first-" .. first)
    assert(saved[weekSlot][1] == 1 and saved[weekSlot][2] == first)
    assert(labels["calendar-weekday-1"] == weekdays[first + 1])
    assert(labels["calendar-weekday-7"] == weekdays[(first + 6) % 7 + 1])
    assert(labels["calendar-date-" .. ((4 - first) % 7 + 1)] == "1")
end

rejectWeek = true
step(0, "calendar-first-1")
assert(saved[weekSlot][2] == 6 and labels["calendar-weekday-1"] == "Sat")
rejectWeek = false
step(0, "calendar-first-1")
assert(saved[weekSlot][2] == 1 and labels["calendar-weekday-1"] == "Mon")
init()
step(0, "timer-stats")
assert(labels["calendar-weekday-1"] == "Mon")

wall = "20250101120000"
step(0)
assert(labels["calendar-month"] == "2025-01" and labels["calendar-date-3"] == "1")

saved[weekSlot] = {2, 6}
init()
step(0, "timer-stats")
step(0, "calendar-first-1")
assert(saved[weekSlot][1] == 2 and saved[weekSlot][2] == 6 and labels["calendar-weekday-1"] == "Sun")

saved[weekSlot] = {1, 1}
saved["work-calendar"] = {1, {
    {"2025", {{"1231", 61}}},
    {"2026", {{"0101", 18}}},
}}
wall = "20260101120000"
init()
step(0, "timer-stats")
assert(labels["calendar-selected-date"] == "2026-01-01" and labels["calendar-selected-time"] == "00:00:18")

step(0, "calendar-prev-month")
assert(labels["calendar-month"] == "2025-12" and labels["calendar-date-1"] == "1")
step(0, "calendar-day-42")
assert(labels["calendar-selected-date"] == "2026-01-01")
step(0, "calendar-day-31")
assert(labels["calendar-selected-date"] == "2025-12-31" and labels["calendar-selected-time"] == "00:01:01")

step(0, "calendar-first-0")
assert(labels["calendar-month"] == "2025-12" and labels["calendar-date-32"] == "31")
assert(labels["calendar-selected-date"] == "2025-12-31")
step(0, "calendar-next-month")
assert(labels["calendar-month"] == "2026-01")
step(0, "calendar-day-5")
step(0, "timer-toggle")
step(2)
assert(labels["calendar-selected-time"] == "00:00:20")
step(0, "calendar-day-6")
assert(labels["calendar-selected-date"] == "2026-01-02" and labels["calendar-selected-time"] == "00:00:00")
assert(saved["work-calendar"][2][1][2][1][2] == 61)

wall = "20250101120000"
init()
step(0, "timer-stats")
step(0, "calendar-prev-month")
assert(labels["calendar-month"] == "2025-01" and labels["calendar-date-4"] == "1")
wall = "29981201120000"
step(0, "calendar-next-month")
assert(labels["calendar-month"] == "2998-12")
