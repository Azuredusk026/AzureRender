local player, language, music
local effects, focus
local present = {}
local focusPanels = {TutorialFocusPomodoro = "opening-tutorial-clock", TutorialFocusNote = "notes-panel",
                     TutorialFocusTodo = "todo-panel"}

function present:begin() effects = {} end
function present:motion() end
function present:update(dt)
    music:update(dt)
    if focus then focus.remaining = focus.remaining - dt end
end

function present:effect(row, delay)
    effects[#effects + 1] = {row.row, delay}
    if row.command == "PlayBGM" or row.command == "TalkLogOnlyPlayBGM" then music:play(row.arguments) end
    local panel = focusPanels[row.command]
    if not panel then return end
    self.host:ui_visible(panel, true)
    focus = {remaining = 3.5}
    return function()
        if focus.remaining > 1e-7 then return false end
        self.host:ui_visible(panel, false)
        focus = nil
        return true
    end
end

function present:finish(chapter, replay, completed)
    assert(completed)
    music:finish()
    assert(self.host:try_write_save("opening-replay-proof", {chapter.group, replay, effects}))
end

local function currentLanguage()
    local saved = self:read_save("room-language")
    if type(saved) == "table" and saved[1] == 1 and opening_tags[saved[2]] then return opening_tags[saved[2]] end
    return opening_tags[opening_language_default]
end

local function currentLevel()
    local saved = self:read_save("work-calendar")
    return type(saved) == "table" and saved[1] == 2 and saved[3][1] or 1
end

function init()
    language = currentLanguage()
    present.host = self
    music = new_story_music(self, opening_music)
    player = new_story_player(self, room_story_chapters,
        new_story_dialogue(self, opening_caption_speeds, function() return language end),
        function() return language end, currentLevel, present, opening_labels)
end

function update(dt)
    local selected = currentLanguage()
    local changed = selected ~= language
    language = selected
    player:update(dt, true, changed)
end
