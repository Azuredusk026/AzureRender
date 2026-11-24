local persisted, blocked
local function copy(value)
    if type(value) ~= "table" then return value end
    local cloned = {}
    for key, item in pairs(value) do cloned[key] = copy(item) end
    return cloned
end
local host = {
    read_save = function(_, slot) assert(slot == "habit-tracker"); return copy(persisted) end,
    try_write_save = function(_, slot, document)
        assert(slot == "habit-tracker")
        if blocked then return false end
        persisted = copy(document)
        return true
    end,
}
local weekdays = {["20261101"] = 0, ["20261102"] = 1, ["20261103"] = 2, ["20261104"] = 3}
local habits = new_room_habits(host, function(day) return weekdays[day] ~= nil end, function(day) return weekdays[day] end)
local first = assert(habits:add("Read", "20261101"))
local second = assert(habits:add("Stretch", "20261102"))
assert(not habits:toggleCompletion(second, "20261101"))
assert(habits:toggleCompletion(first, "20261101"))
assert(habits:completed("20261101")[1] == "Read")
assert(habits:toggleWeekday(first, 1))
assert(not habits:toggleCompletion(first, "20261102"))
assert(habits:togglePause(first, "20261103"))
assert(not habits:toggleCompletion(first, "20261103"))
assert(habits:togglePause(first, "20261104"))
assert(not habits:toggleCompletion(first, "20261103"))
assert(habits:toggleCompletion(first, "20261104"))
assert(habits:reorder(second, -1))
assert(habits:list()[1][1] == second)
blocked = true
assert(not habits:rename(first, "Rejected"))
assert(habits:completed("20261104")[1] == "Read")
blocked = false
assert(habits:rename(first, "Read today"))
assert(habits:archive(first))
assert(#habits:list() == 1 and habits:list()[1][1] == second)
assert(habits:completed("20261104")[1] == "Read today")
assert(not habits:toggleCompletion(first, "20261104"))
persisted = {2, 1, {}}
assert(not habits:add("Unavailable", "20261101") and persisted[1] == 2)
