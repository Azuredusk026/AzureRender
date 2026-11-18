work_reward_config = {
    exp = {minutes = 1, perBlock = 10, maxHours = 12},
    points = {minutes = 1, perBlock = 10, maxHours = 12},
    levelThresholds = {1, 250, 350},
    fallbackThreshold = 3000,
}

local persisted = {1, {{"2026", {{"1117", 10}}}}}
local failWrite = false
self = {
    local_datetime = function() return "20261117004500" end,
    read_save = function() return persisted end,
    try_write_save = function(_, _, document)
        if failWrite then return false end
        persisted = document
        return true
    end,
}

local calendar = new_work_calendar()
assert(calendar:totalTime() == 10 and calendar:progress()[1] == 1)
calendar:advance(3, true)
assert(calendar:settle())
assert(persisted[1] == 2 and persisted[4] == 0)
assert(calendar:progress()[1] == 2 and calendar:totalTime() == 13)
calendar:advance(60, true)
assert(persisted[4] == 60 and persisted[3][3] == 0)

local reopened = new_work_calendar()
failWrite = true
reopened:recover()
assert(persisted[4] == 60 and persisted[3][3] == 0)
failWrite = false
assert(reopened:flush())
assert(persisted[4] == 0 and persisted[3][2] == 10 and persisted[3][3] == 10)
assert(reopened:totalTime() == 73)
local again = new_work_calendar()
again:recover()
assert(persisted[3][3] == 10 and again:totalTime() == 73)

again:advance(3, true)
assert(again:setTimeFor("20261116", 86400))
assert(persisted[4] == 3 and persisted[3][3] == 10)
assert(again:timeFor("20261117") == 76 and again:timeFor("20261116") == 86400)
