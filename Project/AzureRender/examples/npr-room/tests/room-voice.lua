local text, visible = {}, {}
local pressed, playing, cursor = false, false, 0
local selected, starts = nil, 0
local source = {}
function source:set(_, key, value)
    assert(key == "asset")
    selected = value
end
function source:audio_play() starts, cursor, playing = starts + 1, 0, true end
function source:try_audio_play() self:audio_play(); return true end
function source:audio_playing() return playing end
function source:audio_position() return cursor end
self = {}
function self:find(node) assert(node == "room-voice"); return source end
function self:pressed(action) assert(action == "room-voice-talk"); return pressed end
function self:ui_text(id, value) text[id] = value end
function self:ui_visible(id, value) visible[id] = value end

local step = new_room_voice()
pressed = true
step(1/60, false)
assert(starts == 0 and not visible["room-voice-talk"])
local played = {}
for index = 1, 23 do
    step(1/60, true)
    assert(starts == index and selected and not played[selected])
    assert(visible["room-voice-talk"] and visible["room-voice-caption"] and text["room-voice-line"] ~= "")
    played[selected] = true
    playing = false
    step(1/60, true)
    assert(starts == index and visible["room-voice-caption"], "A not-yet-started device is not the end of the clip")
    cursor = 60
    step(1/60, true)
    step(.5, true)
    assert(starts == index and visible["room-voice-caption"])
    step(.5, true)
    assert(not visible["room-voice-caption"])
end
local last = selected
step(1/60, true)
assert(starts == 24 and selected ~= last)
