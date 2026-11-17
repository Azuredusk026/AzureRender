local samples = {}
local function capture(ratio, seconds)
    samples[#samples + 1] = {ratio, seconds}
end
local function near(actual, expected)
    assert(math.abs(actual - expected) < 1e-9)
end

local effect = new_story_ratio("DirectionChromaticAberration", capture)
local rise = {row = 2, command = "DirectionChromaticAberration", arguments = {"0.2", "2", "isSine", "0"}}
local clear = {row = 20, command = "DirectionChromaticAberration", arguments = {"0", "0.2", "isSine", "0"}}
local tutorial = {category = 0, commands = {rise, {command = "Text"}, clear}}

effect:begin(tutorial, false)
effect:effect(rise, 0.5)
effect:update(0.5)
assert(#samples == 0)
effect:update(1)
near(samples[#samples][1], 0.1)
near(samples[#samples][2], 1.5)
effect:update(1)
near(samples[#samples][1], 0.2)
effect:effect(clear, 0)
effect:update(0.1)
near(samples[#samples][1], 0.1)
effect:update(0.1)
near(samples[#samples][1], 0)
near(samples[#samples][2], 2.7)

effect:begin(tutorial, true)
near(samples[#samples][1], 0.2)
local count = #samples
effect:effect(rise, 0)
effect:update(0.5)
assert(#samples == count)
effect:effect(clear, 0.5)
effect:finish()
near(samples[#samples][1], 0)
count = #samples
effect:update(1)
assert(#samples == count)

effect:begin({category = 1, commands = {rise, {command = "Text"}, clear}}, false)
near(samples[#samples][1], 0.2)
effect:finish()
near(samples[#samples][1], 0)
