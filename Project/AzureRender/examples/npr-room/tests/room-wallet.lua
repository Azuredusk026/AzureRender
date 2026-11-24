local earned = {2, {}, {12, 0, 13999}, 0}
local grants, ledger, writes, reject = nil, nil, 0, false
local host = {}

function host:read_save(slot)
    if slot == "work-calendar" then return earned end
    if slot == "room-unlocks" then return grants end
    assert(slot == "room-purchases")
    return ledger
end
function host:try_write_save(slot, value)
    assert(slot == "room-purchases")
    writes = writes + 1
    if reject then return false end
    ledger = value
    return true
end
function host:local_datetime() return "20261117110000" end

local catalog = {City = {14000, ""}, Horizon = {6000, ""}, Expired = {1, "20261001235959"}}
local wallet = new_room_wallet(host, catalog)
assert(wallet:balance() == 13999)
assert(select(2, wallet:purchase("City")) == "insufficient" and writes == 0 and not ledger)
assert(select(2, wallet:purchase("Expired")) == "unavailable" and writes == 0)
assert(select(2, wallet:purchase("Missing")) == "unavailable" and writes == 0)
earned[3][3] = 14000
reject = true
assert(select(2, wallet:purchase("City")) == "save-failed" and writes == 1 and not ledger)
assert(wallet:balance() == 14000 and not wallet:owns("City"))
reject = false
assert(wallet:purchase("City") and wallet:owns("City") and wallet:balance() == 0)
assert(ledger[2][1][1] == "City" and ledger[2][1][2] == 14000 and #ledger[2] == 1)
assert(not wallet:purchase("City") and writes == 2)
local reopened = new_room_wallet(host, catalog)
assert(reopened:owns("City") and reopened:balance() == 0)
earned[3][3] = 20000
grants = {1, {}, {"Horizon"}}
assert(select(2, reopened:purchase("Horizon")) == "owned" and writes == 2)
assert(reopened:balance() == 6000)
ledger = {2, {{"City", 14000}}}
assert(select(2, reopened:purchase("Horizon")) == "save-failed" and writes == 2)
