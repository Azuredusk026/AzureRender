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

played = {}
local chapter = {group = "main_01", commands = {row("Text", 0)}}
motion = new_story_body(select, {["0"] = "legacy-base", ["12"] = "nod"}, {
    main_01 = {entry = "main-base", signals = {["0"] = "main-0", ["5"] = "main-5", ["1003"] = "main-1003"}}
})
motion:begin(chapter)
motion:row(chapter.commands[1])
motion:row(row("Text", 5))
motion:row(row("Text", -1))
motion:row(row("Text", 0))
motion:row(row("Text", 1003))
motion:finish()
motion:begin({group = "tutorial_01", commands = {row("Text", 12)}})
assert(table.concat(played, ",") == "main-base,main-5,main-0,main-1003,idle,nod")
