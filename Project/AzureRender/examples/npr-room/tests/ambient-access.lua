local progress
local clock = "20260430235959"
self = {}
function self:read_save(slot) assert(slot == "room-unlocks"); return progress end
function self:local_datetime() return clock end
local access = new_ambient_access()
assert(access:state(8) == "locked")
assert(access:state(9) == "open")
assert(access:state(25) == "locked")
assert(access:state(28) == "locked")
clock = "20260501000000"
access:refresh()
assert(access:state(28) == "expired")
progress = {1, {"main_19", "special_nearspring2026_scenario_001"}, {"City"}}
access:refresh()
assert(access:state(8) == "open")
assert(access:state(25) == "open")
assert(access:state(28) == "open")
progress = {2, {"main_19"}, {"City"}}
access:refresh()
assert(access:state(8) == "locked" and access:state(25) == "locked")
