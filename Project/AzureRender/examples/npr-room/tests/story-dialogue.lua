utf8 = nil
local language = "ZhHans"
local source, text, visible = {}, {}, {}
local starts, stops, position, playing = 0, 0, 0, false
source.set = function(_, component, property, asset)
    assert(component == "azure.audio-source" and property == "asset" and asset == "assets:/voice/native.ogg")
    position = 0
end
source.audio_play = function() starts, playing = starts + 1, true end
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
