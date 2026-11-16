local room = "assets:/room.azurelevel"
local events = {}
local actions = {}
local status
local ready = false
local failure = ""

self = {
    load_level = function(_, level) events[#events + 1] = "load " .. level end,
    preload_level = function(_, level) events[#events + 1] = "preload " .. level end,
    cancel_level_load = function() events[#events + 1] = "cancel" end,
    level_ready = function(_, level) assert(level == room); return ready end,
    level_load_error = function() return failure end,
    pressed = function(_, action) return actions[action] end,
    ui_text = function(_, id, text) assert(id == "transition-status"); status = text end,
}

for stage, next_stage in pairs({startup = "asset-load", ["asset-load"] = "entry", entry = "transition"}) do
    self.id = stage
    init()
    assert(table.remove(events) == "load assets:/" .. next_stage .. ".azurelevel")
end

self.id = "transition"
init()
assert(table.remove(events) == "preload " .. room)
failure = "missing asset"
update(1 / 60)
assert(status:find(failure, 1, true))
actions["transition-retry"] = true
update(1 / 60)
assert(table.remove(events) == "preload " .. room)
actions["transition-retry"] = false
failure = ""
actions["transition-cancel"] = true
update(1 / 60)
assert(table.remove(events) == "cancel")
actions["transition-cancel"] = false
ready = true
for _ = 1, 240 do update(1 / 60) end
assert(#events == 0)

actions["transition-retry"] = true
update(1 / 60)
assert(table.remove(events) == "preload " .. room)
actions["transition-retry"] = false
ready = false
for _ = 1, 180 do update(1 / 60) end
assert(#events == 0)
ready = true
for _ = 1, 24 do update(1 / 60) end
assert(#events == 0)
for _ = 1, 8 do update(1 / 60) end
assert(table.remove(events) == "load " .. room)
update(1 / 60)
assert(#events == 0)
