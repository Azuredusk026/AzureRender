utf8 = nil
local language = "ZhHans"
local source, text, visible = {}, {}, {}
local starts, stops, position, playing = 0, 0, 0, false
local available = true
source.set = function(_, component, property, asset)
    assert(component == "azure.audio-source" and property == "asset" and asset == "assets:/voice/native.ogg")
    position = 0
end
source.audio_play = function()
    if not available then error("Voice unavailable") end
    starts, playing = starts + 1, true
end
source.try_audio_play = function()
    if not available then return false end
    source.audio_play()
    return true
end
source.audio_stop = function() stops, playing = stops + 1, false end
source.audio_playing = function() return playing end
source.audio_position = function() return position end
local host = {
    find = function(_, node) assert(node == "opening-story-voice"); return source end,
    ui_text = function(_, node, value) text[node] = value end,
    ui_visible = function(_, node, value) visible[node] = value end,
}
local dialogue = new_story_dialogue(host, {ZhHans = .13, Ru = .02}, function() return language end)
local line = {translations = {ZhHans = "中文字符", Ru = "Текст"}, asset = "voice/native.ogg",
              frames = 48000, sampleRate = 48000}
dialogue:show(line)
assert(text["opening-story-line"] == "中" and visible["opening-story-caption"] and starts == 1)
dialogue:update(.12)
assert(text["opening-story-line"] == "中")
dialogue:update(.01)
assert(text["opening-story-line"] == "中文")
dialogue:fastForward()
dialogue:update(.03)
assert(text["opening-story-line"] == "中文字符" and not dialogue:ready())
language = "Ru"
dialogue:languageChanged()
assert(text["opening-story-line"] == "Текст" and starts == 1)
playing, position = false, 1
dialogue:update(0)
assert(dialogue:ready())
dialogue:clear()
assert(not visible["opening-story-caption"] and stops == 0)
dialogue:show(line)
dialogue:clear()
assert(stops == 1 and not dialogue:ready())

available = false
playing, position = false, 0
dialogue:show(line)
dialogue:fastForward()
dialogue:update(.5)
assert(not dialogue:ready(), "Unavailable voice must retain the declared presentation duration")
dialogue:update(.5)
assert(dialogue:ready() and visible["opening-story-caption"], "Unavailable voice must leave text dialogue advanceable")
dialogue:clear()
available = true
dialogue:show(line)
assert(playing, "The next line must retry a recovered voice")
