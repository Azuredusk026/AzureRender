local visible, texts, actions = {}, {}, {}
local saved, asset, position, playing
local starts = 0
local source = {}
function source:set(_, key, value) assert(key == "asset"); asset = value end
function source:audio_play() starts, position, playing = starts + 1, 0, true end
function source:audio_playing() return playing end
function source:audio_position() return position end
self = {}
function self:find(id) assert(id == "room-choice-voice"); return source end
function self:pressed(id) return actions[id] end
function self:ui_text(id, value) texts[id] = value end
function self:ui_visible(id, value) visible[id] = value end
function self:read_save(slot) assert(slot == "work-conversation"); return saved end
function self:try_write_save(slot, value) assert(slot == "work-conversation"); saved = value; return true end

local roll = .1
local story = new_work_choice(function(limit) if limit then return 1 else return roll end end)
local function tap(action)
    actions[action] = true
    local consumed = story:step(1/60, true)
    actions = {}
    return consumed
end
local function playToEnd()
    playing, position = false, 60
    assert(story:step(1/60, true))
    assert(story:step(2, true))
end

story:recordWork(60)
story:complete()
assert(not tap("room-voice-talk") and starts == 0)
roll = .04
assert(tap("room-voice-talk"))
assert(saved[2] == 1 and saved[3] == 1 and starts == 1)
playToEnd()
assert(saved[3] == 2 and visible["room-choice-options"])
story = new_work_choice(function() error("A saved story must open without another draw") end)
assert(tap("room-choice-option-2"))
assert(saved[3] == 3 and saved[4] == 2 and not visible["room-choice-options"])
story:step(2, true)
assert(starts == 2 and visible["room-choice-caption"])
playToEnd()
assert(saved[2] == 0 and not visible["room-choice-caption"])
assert(not tap("room-voice-talk"))

local responses = {}
for episode = 1, 20 do
    for option = 1, 3 do
        saved = {1, episode, 2, 0}
        story = new_work_choice()
        assert(tap("room-choice-option-" .. option))
        assert(saved[2] == episode and saved[4] == option)
        story:step(2, true)
        assert(asset and not responses[asset])
        responses[asset] = true
        playToEnd()
        assert(saved[2] == 0)
    end
end
