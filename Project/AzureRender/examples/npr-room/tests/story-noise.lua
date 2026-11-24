local opacity = 0
local source = {set = function(_, component, property, value)
    assert(component == "azure.screen-noise" and property == "opacity")
    opacity = value
end}
local host = {find = function(_, id) if id == "opening-story-noise" then return source end end}
local noise = new_story_noise(host)
local function command(row, opacity, duration, ease)
    return {row = row, command = "DirectionMonochromeNoise", arguments = {opacity, duration, ease}}
end
local tutorialOpening = command(3, ".05", "2", "isSine")
local tutorial = {category = 18, commands = {tutorialOpening, {command = "WaitForSeconds"}}}
local chapterOpening = command(93, ".03", ".05", "isSine")
local chapter = {category = 1, commands = {chapterOpening, {command = "WaitForSeconds"}}}
local function close(expected) assert(math.abs(opacity - expected) < 1e-7, tostring(opacity)) end

noise:begin(tutorial, false)
noise:effect(tutorialOpening, 0)
close(0)
noise:update(.5)
close(.0125)
noise:update(1.5)
close(.05)

noise:effect(command(34, "0", "0", "isSine"), 1.3)
noise:effect(command(32, ".02", ".5", "isSine"), .9)
noise:update(1.15)
close(.035)
noise:update(.05)
close(.032)
noise:update(.2)
close(0)

noise:effect(command(88, ".05", ".1", "0"), 0)
noise:update(.05)
close(.0375)
noise:effect(command(90, ".2", "1", "isSine"), 1)
noise:finish()
noise:update(3)
close(0)

noise:begin(tutorial, true)
close(.05)
noise:effect(tutorialOpening, 0)
noise:effect(command(21, "0", ".2", "isSine"), 0)
noise:update(.1)
close(.025)
noise:update(.1)
close(0)
noise:finish()

noise:begin(chapter, false)
close(.03)
noise:effect(chapterOpening, 0)
noise:effect(command(95, "0", ".5", "isSine"), 0)
noise:update(.25)
close(.015)
noise:update(.25)
close(0)
