local samples = {}
local function observe(ratio, blind, time)
    assert(not blind)
    samples[#samples + 1] = {ratio, time}
end

local command = "DirectionScreenMove"
local effect = new_story_hold(command, observe, false)
local tutorialMove = {row = 14, command = command, arguments = {"0.5", "1", "0", "0"}}
local hold = {row = 1, command = command, arguments = {"0.25", "-1", "1", "0"}}
local chapter = {category = 0, commands = {tutorialMove, {command = "WaitForSeconds"}}}

effect:begin(chapter, false)
effect:effect(tutorialMove, .7)
for _ = 1, 41 do effect:update(1 / 60) end
assert(#samples == 0)
effect:update(1 / 60)
assert(samples[1][1] == .5 and math.abs(samples[1][2] - .7) < 1e-9)
for _ = 1, 59 do effect:update(1 / 60) end
assert(#samples == 1)
effect:update(1 / 60)
assert(samples[2][1] == 0 and math.abs(samples[2][2] - 1.7) < 1e-9)

effect:begin(chapter, true)
effect:effect(tutorialMove, 0)
effect:update(2)
assert(#samples == 2)
effect:begin({category = 1, commands = {hold, {command = "Text"}}}, false)
assert(samples[3][1] == .25 and samples[3][2] == 0)
effect:effect(tutorialMove, .7)
effect:finish()
effect:update(2)
assert(#samples == 4 and samples[4][1] == 0)
