local played = {}
local function select(state) played[#played + 1] = state end
local motion = new_story_body(select, {["0"] = "base", ["12"] = "nod", ["401"] = "start-end"})
local function row(command, number)
    return {command = command, bodyMotion = number}
end
local first = row("Text", -1)
motion:begin({commands = {row("WaitForSeconds"), first}})
motion:row(first)
motion:row(row("ChangeMotion", 401))
motion:row(row("Text", 12))
motion:row(row("Text", -1))
motion:row(row("Text", 12))
motion:row(row("Text", 0))
motion:row(row("Text", 12))
motion:finish()
assert(table.concat(played, ",") == "base,start-end,nod,base,nod,idle")
motion:begin({commands = {row("Text", 12)}})
assert(played[#played] == "nod")
motion:finish()
assert(played[#played] == "idle")
