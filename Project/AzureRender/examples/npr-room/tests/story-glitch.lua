local ratios, camera, intervals = {}, {}, {.3, .4, .3, .4, .3}
local selections = 0
local effect = new_story_glitch(function(value) ratios[#ratios + 1] = value end,
    function(enabled) camera[#camera + 1] = enabled end,
    function(minimum, maximum)
        assert(minimum == .3 and maximum == 1)
        selections = selections + 1
        return intervals[selections]
    end)
local start = {row = 12, command = "DirectionGlitch1", arguments = {"0.5", "1", "0", "0"}}
local subsequent = {row = 28, command = "DirectionGlitch1", arguments = {"0.3", "0.7", "0", "0"}}
local chapter = {category = 0, commands = {start, {command = "WaitForSeconds"}, subsequent}}

effect:begin(chapter, false)
effect:effect(start, .7)
effect:update(.69)
assert(#camera == 0 and #ratios == 0 and selections == 0)
effect:update(.01)
assert(camera[1] == true and ratios[1] == .5 and selections == 1)
effect:update(.3)
assert(camera[2] == false and selections == 2)
effect:update(.4)
assert(camera[3] == true and selections == 3)
effect:update(.3)
assert(camera[4] == false and ratios[2] == 0 and selections == 3)

effect:effect(subsequent, 0)
effect:update(1 / 60)
assert(camera[5] == true and ratios[3] == .3 and selections == 4)
effect:finish()
effect:update(2)
assert(camera[6] == false and ratios[4] == 0 and selections == 4)

local hold = {row = 1, command = "DirectionGlitch1", arguments = {"0.5", "-1", "0", "0"}}
effect:begin({category = 0, commands = {hold, {command = "Text"}}}, true)
assert(ratios[5] == .5 and #camera == 6 and selections == 4)
effect:effect(hold, 0)
effect:update(1)
assert(#camera == 6 and selections == 4)
effect:finish()
assert(ratios[6] == 0 and #camera == 6)
