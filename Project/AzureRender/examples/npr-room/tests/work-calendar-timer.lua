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
