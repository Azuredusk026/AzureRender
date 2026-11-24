local focused, events, captured = {}, {}, {}
local host = {
    ui_visible = function(_, id, value) focused[id] = value end
}
local function record(event) events[#events + 1] = event end
local music = {
    update = function(_, dt) assert(dt == .5); record("music.update") end,
    play = function(_, args) assert(args[1] == "Nostalgia"); record("music.play") end,
    finish = function() record("music.finish") end
}
local noise = {
    begin = function(_, chapter, replay) assert(chapter.group == "tutorial_01" and replay); record("noise.begin") end,
    update = function(_, dt) assert(dt == .5); record("noise.update") end,
    effect = function(_, row, delay) assert(row.row >= 0 and delay == .7); record("noise.effect") end,
    finish = function() record("noise.finish") end
}
function new_story_music(h, tracks) assert(h == host and tracks.Nostalgia); return music end
function new_story_noise(h) assert(h == host); return noise end
function new_story_dialogue(h, speeds, language)
    assert(h == host and speeds.Ja and language() == "Ja")
    return {clear = function() end}
end
function new_story_player(h, chapters, dialogue, language, level, presentation, labels)
    assert(h == host and #chapters == 1 and dialogue and language() == "Ja" and level() == 2 and labels.log)
    captured.presentation = presentation
    return {update = function(_, dt, available, changed) return dt == .5 and available and changed end}
end
local chapter = {group = "tutorial_01"}
local controller = new_story_room(host, {chapter}, {Nostalgia = "source"}, {Ja = .13}, {log = {}},
    function() return "Ja" end, function() return 2 end)
assert(controller:update(.5, true, true) and not controller:update(.5, false, true))
local p = captured.presentation
p:begin(chapter, true)
assert(focused["opening-story-input-mask"])
p:effect({row = 0, command = "TalkLogOnlyPlayBGM", arguments = {"Nostalgia"}}, .7)
local ready = p:effect({row = 1, command = "TutorialFocusNote"}, .7)
assert(focused["opening-focus-notes"] == true and not ready())
for _ = 1, 6 do p:update(.5); assert(not ready()) end
p:update(.5)
assert(ready() and focused["opening-focus-notes"] == false)
local nextFocus = p:effect({row = 2, command = "TutorialFocusTodo"}, .7)
assert(not nextFocus() and focused["opening-focus-todo"])
p:finish(chapter, true, false)
assert(focused["opening-focus-todo"] == false and not focused["opening-focus-clock"])
assert(focused["opening-story-input-mask"] == false)
assert(events[1] == "noise.begin" and events[3] == "music.play")
assert(events[#events - 1] == "music.finish" and events[#events] == "noise.finish")
