local tutorial, first = table.unpack(room_story_chapters)
local save = {1, {}, {"City"}, {}}
local pressed, visible, text = {}, {}, {}
local language, level = "ZhHans", 1
local host = {
    pressed = function(_, action) return pressed[action] end,
    ui_text = function(_, id, value) text[id] = value end,
    ui_visible = function(_, id, value) visible[id] = value end,
    read_save = function(_, slot) assert(slot == "room-unlocks"); return save end,
    try_write_save = function(_, slot, value) assert(slot == "room-unlocks"); save = value; return true end,
}
local voiced, effects, begun, finished = {}, {}, {}, {}
local revealed, switched = 0, 0
local dialogue = {
    show = function(_, row) voiced[#voiced + 1] = row.row end,
    clear = function() end,
    ready = function() return true end,
    update = function() end,
    fastForward = function() revealed = revealed + 1 end,
    languageChanged = function() switched = switched + 1 end,
}
local presentation = {
    begin = function(_, chapter, replay) begun[#begun + 1] = {chapter.group, replay} end,
    finish = function(_, chapter, replay, completed) finished[#finished + 1] = {chapter.group, replay, completed} end,
    motion = function() end,
    update = function() end,
    effect = function(_, row, delay) effects[row.row] = delay end,
}
local player = new_story_player(host, room_story_chapters, dialogue, function() return language end,
    function() return level end, presentation, {})
local function tick(seconds, action, available, languageChanged)
    pressed = action and {[action] = true} or {}
    return player:update(seconds, available ~= false, languageChanged)
end
local function waitUntil(id)
    for _ = 1, 100 do
        if visible[id] then return end
        tick(.25)
    end
    error("Story never displayed " .. id)
end
assert(not tick(0, "opening-story-start", false) and #begun == 0)
assert(tick(0, "opening-story-start") and begun[1][1] == "tutorial_01" and not begun[1][2])
assert(not visible["opening-story-start"] and visible["opening-story-auto"])
waitUntil("opening-story-options")
assert(visible["opening-story-option-1"] and not visible["opening-story-option-2"])
assert(text["opening-story-option-label-1"] == tutorial.commands[44].translations.ZhHans)
language = "Ru"
tick(0, nil, nil, true)
assert(switched == 1 and text["opening-story-option-label-1"] == tutorial.commands[44].translations.Ru)
tick(0, "opening-story-option-1")
waitUntil("opening-story-heroine")
local voicedBefore = #voiced
tick(10, "opening-story-next")
assert(visible["opening-story-heroine"] and #voiced == voicedBefore)
tick(0, "opening-story-heroine")
waitUntil("opening-story-options")
assert(visible["opening-story-option-4"])
tick(0, "opening-story-option-1")
waitUntil("opening-story-options")
assert(visible["opening-story-option-4"] and visible["opening-story-option-used-1"])
tick(0, "opening-story-option-4")
waitUntil("opening-story-options")
assert(visible["opening-story-option-2"] and not visible["opening-story-option-4"])
tick(0, "opening-story-option-2")
waitUntil("opening-story-log")
assert(save[2][1] == tutorial.group and save[3][1] == "City")
assert(finished[1][1] == tutorial.group and not finished[1][2] and finished[1][3])
assert(not visible["opening-story-start"] and not tick(0, "opening-story-start"))
level = 2
tick(0)
assert(visible["opening-story-start"] and text["opening-story-start"] == first.title.Ru)
tick(0, "opening-story-start")
waitUntil("opening-story-log")
assert(save[2][2] == first.group and effects[90] == 0 and #finished == 2)
tick(0, "opening-story-log")
assert(visible["opening-story-replay-1"] and visible["opening-story-replay-2"])
tick(0, "opening-story-replay-1")
assert(begun[3][2])
tick(.25)
assert(effects[0] == 0)
