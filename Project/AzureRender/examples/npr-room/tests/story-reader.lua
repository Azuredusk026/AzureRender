local saved = {1, {}, {"City"}, {"pending-unlock"}}
local written, blockWrite = 0, false
local host = {
    read_save = function(_, name)
        assert(name == "room-unlocks")
        return saved
    end,
    try_write_save = function(_, name, value)
        assert(name == "room-unlocks")
        if blockWrite then return false end
        saved, written = value, written + 1
        return true
    end,
}
local tutorial = {group = "opening", category = 18, episode = 1, unlockLevel = 0, commands = {
    {command = "TalkLogOnlyPlayBGM", arguments = {"Nostalgia"}},
    {command = "WaitForSeconds", seconds = 1},
    {command = "ReadNextForScript"},
    {command = "StartSelectOnce"},
    {command = "TutorialSelection", usedKey = 1, target = 7},
    {command = "TutorialSelection", usedKey = 5, target = 11},
    {command = "Text", delay = 0.5, hold = 0.7, voice = "Response"},
    {command = "TutorialFocusNote"},
    {command = "WaitForSeconds", seconds = 0.5},
    {command = "Jump", target = 5},
    {command = "EndScenario"},
}}
local first = {group = "first", category = 1, episode = 1, unlockLevel = 2, commands = {
    {command = "SetDirectionDelaySeconds", arguments = {"0.7"}},
    {command = "DirectionMonochromeNoise", arguments = {".05", ".05", "isSine"}},
    {command = "Text", delay = 0, hold = 1},
}}
local second = {group = "second", category = 1, episode = 2, unlockLevel = 3}
local chapters = {tutorial, first, second}
local progress = new_story_progress(host, chapters)
assert(progress:nextChapter(1) == tutorial)
assert(not progress:canStart(first, 2, false))
assert(not progress:canStart(second, 20, false))

local effects, ended, phases = {}, {}, {}
local done, focusDone, completed = false, false, false
local driver = {
    begin = function(_, chapter, replay) assert(chapter == tutorial or chapter == first) end,
    motion = function() end,
    text = function(_, row) phases[#phases + 1] = row end,
    textReady = function() return done end,
    reveal = function() done = true end,
    clear = function() end,
    choices = function(_, choices, used)
        assert(#choices == 2 and choices[1].target == 7 and choices[2].target == 11)
        if completed then assert(used[1] and not used[5]) end
    end,
    effect = function(_, row, delay)
        effects[#effects + 1] = {row.command, delay}
        if row.command == "TutorialFocusNote" then return function() return focusDone end end
    end,
    finish = function(_, chapter, replay) ended[#ended + 1] = {chapter, replay} end,
}
local story = new_room_story(driver, progress)
assert(story:start(tutorial, 1, false))
story:update(0, "next")
assert(#effects == 0 and story:state() == "wait")
story:update(1, "next")
assert(story:state() == "script")
story:update(0, "next")
assert(story:state() == "script")
story:update(0, "script")
assert(story:state() == "choice")
story:update(0, 1)
story:update(.49)
assert(#phases == 0)
story:update(.01)
assert(#phases == 1 and phases[1].voice == "Response")
story:update(0, "next")
assert(done and story:state() == "text")
story:update(0, "next")
assert(story:state() == "effect" and effects[1][1] == "TutorialFocusNote")
completed, focusDone = true, true
story:update(0)
story:update(.5)
assert(story:state() == "choice")
story:update(0, 1)
assert(story:state() == "delay")
story:update(.5)
story:update(0, "next")
story:update(0)
story:update(.5)
assert(story:state() == "choice")
blockWrite = true
story:update(0, 2)
assert(story:state() == "persist" and written == 0 and #ended == 0)
blockWrite = false
story:update(0, "next")
assert(story:state() == "idle" and written == 1 and #ended == 1)
assert(saved[2][1] == "opening" and saved[3][1] == "City" and saved[4][1] == "pending-unlock")
assert(progress:nextChapter(1) == nil and progress:nextChapter(2) == first)

assert(story:start(tutorial, 1, true))
story:update(0)
assert(effects[3][1] == "TalkLogOnlyPlayBGM")
story:cancel()
assert(story:state() == "idle" and written == 1)

assert(story:start(first, 2, false))
story:update(0)
assert(effects[4][1] == "DirectionMonochromeNoise" and effects[4][2] == .7)
story:update(.99, "auto")
assert(story:state() == "text")
story:update(.01)
assert(story:state() == "idle" and saved[2][2] == "first")
assert(progress:nextChapter(2) == nil and progress:nextChapter(3) == second)

saved = {2, {"opening"}}
assert(not progress:canStart(tutorial, 1, false) and not progress:complete(tutorial) and written == 2)
