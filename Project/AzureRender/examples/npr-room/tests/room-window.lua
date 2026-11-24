local available, purchased, reject = false, false, false
local save, visible, writes = nil, nil, 0
local node, host = {}, {}
function node:set(kind, key, value)
    assert(kind == "azure.renderable" and key == "visible")
    visible = value
end
function host:find(name) assert(name == "city-node"); return node end
function host:read_save(name) assert(name == "room-windows"); return save end
function host:try_write_save(name, value)
    assert(name == "room-windows")
    writes = writes + 1
    if reject then return false end
    save = value
    return true
end
local function reopen()
    return new_room_window(host, "City", "city-node", function() return available end,
                           function() return purchased end)
end
local city = reopen()
assert(not visible and not city:active())
assert(not city:set(true) and writes == 0)
available, purchased = true, true
city:restore()
assert(visible and city:active() and writes == 0)
reject = true
assert(not city:set(false) and visible and not save and writes == 1)
reject = false
assert(city:set(false) and not visible and not city:active() and writes == 2)
assert(save[2][1][1] == "City" and save[2][1][2] == false)
city = reopen()
assert(not visible and not city:active())
assert(city:set(true) and visible)
save = {2, {}}
assert(not city:set(false) and visible and save[1] == 2)
