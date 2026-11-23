local labels = {}
for _, name in ipairs({"language", "start", "pause", "resume", "finish", "statistics",
                       "pomodoro", "countup", "working", "resting", "work", "rest", "loop"}) do
    local values = {}
    for index = 1, 7 do values[index] = name .. index end
    labels[name] = values
end

local saved, attempted, actions, visible, displayed, denied
local function reset(value)
    saved, attempted, actions, visible, displayed, denied = value, 0, {}, {}, {}, false
end

self = {
    read_save = function(_, slot) assert(slot == "room-language"); return saved end,
    try_write_save = function(_, slot, value)
        assert(slot == "room-language")
        attempted = attempted + 1
        if denied then return false end
        saved = value
        return true
    end,
    pressed = function(_, key) return actions[key] == true end,
    ui_visible = function(_, id, value) visible[id] = value end,
    ui_text = function(_, id, text) displayed[id] = text end,
}

local config = {default = 3, strings = labels}
local transcript = {"日", "English", "简体", "繁體", "Português", "한국어", "Русский"}
reset({1, 99})
local language = new_room_language(config)
assert(language:text(transcript) == transcript[3] and language:text("unchanged") == "unchanged")
assert(displayed["room-language-toggle"] == "language3")

actions["room-language-2"] = true
assert(not language:update() and attempted == 0)
actions = {["room-language-toggle"] = true}
assert(not language:update() and visible["room-language-panel"])

for index = 1, 7 do
    actions = {["room-language-" .. index] = true}
    assert(language:update())
    assert(saved[1] == 1 and saved[2] == index)
    assert(language:text(transcript) == transcript[index])
    assert(displayed["room-language-toggle"] == "language" .. index)
    local phase, settings = language:timer("work", false, false, {25, 5, 2})
    assert(phase == "working" .. index and settings:find("loop" .. index, 1, true))
    assert(displayed["timer-toggle-button"] == "pause" .. index .. " · T")
    phase = language:timer("work", true, true, {25, 5, 2})
    assert(phase == "pause" .. index)
    assert(displayed["timer-toggle-button"] == "resume" .. index .. " · T")
end

denied = true
actions = {["room-language-2"] = true}
assert(not language:update() and saved[2] == 7 and language:text(transcript) == transcript[7])
local reopened = new_room_language(config)
assert(reopened:text(transcript) == transcript[7] and not visible["room-language-panel"])

local future = {2, "future locale"}
reset(future)
language = new_room_language(config)
actions = {["room-language-toggle"] = true}; language:update()
actions = {["room-language-2"] = true}
assert(not language:update() and saved == future, "Future language payload must remain intact")
saved = {1,3}
assert(language:update() and saved[1] == 1 and saved[2] == 2)
future = {3, "external future locale"}; saved = future
actions = {["room-language-1"] = true}
assert(not language:update() and saved == future and language:text(transcript) == transcript[2])

local locale, starts, asset = 2, 0, nil
local function translate(value) return value[locale] end
local audio = {}
function audio:set(_, property, value) assert(property == "asset"); asset = value end
function audio:audio_play() starts = starts + 1 end
function audio:try_audio_play() self:audio_play(); return true end
function audio:audio_playing() return true end
function self:find(id)
    assert(id == "room-voice" or id == "room-choice-voice")
    return audio
end

responses = {{"assets:/idle.ogg", transcript, 1, 0, 1}}
local idle = new_room_voice(translate)
actions = {["room-voice-talk"] = true}
assert(idle(1/60, true, false) and starts == 1 and asset == "assets:/idle.ogg")
assert(displayed["room-voice-line"] == transcript[2])
locale = 7
actions = {}
assert(idle(1/60, true, false, true) and starts == 1 and displayed["room-voice-line"] == transcript[7])

local reply = {"返信", "Reply", "回复", "回覆", "Resposta", "답변", "Ответ"}
work_conversations = {{responses[1], {{reply, {"assets:/reply.ogg", reply, 1, 0, 1}}}}}
function self:read_save(slot) assert(slot == "work-conversation"); return {1, 1, 2, 0} end
function self:try_write_save(slot, value) assert(slot == "work-conversation"); saved = value; return true end
local story = new_work_choice(nil, translate)
assert(visible["room-choice-options"] and displayed["room-choice-question"] == transcript[7])
assert(displayed["room-choice-option-1"] == reply[7])
locale = 3
assert(story:step(1/60, true, true) and displayed["room-choice-question"] == transcript[3])
assert(displayed["room-choice-option-1"] == reply[3])
actions = {["room-choice-option-1"] = true}
assert(story:step(1/60, true) and saved[3] == 3)
assert(story:step(1/60, true) and starts == 2 and asset == "assets:/reply.ogg")
assert(displayed["room-choice-line"] == reply[3])
locale = 6
assert(story:step(1/60, true, true) and starts == 2 and displayed["room-choice-line"] == reply[6])
