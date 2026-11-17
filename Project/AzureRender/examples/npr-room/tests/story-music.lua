local playlistPlaying, playlistPosition, gain = true, 37, nil
local musicPlaying, currentAsset = false, nil
local transitions = {}
local playlist = {
    audio_playing = function() return playlistPlaying end,
    audio_pause = function(_, paused) playlistPlaying = not paused; transitions[#transitions + 1] = paused end,
    audio_position = function() return playlistPosition end,
}
local source = {
    set = function(_, component, property, value)
        assert(component == "azure.audio-source")
        if property == "asset" then currentAsset = value
        elseif property == "volume" then gain = value
        else error(property) end
    end,
    audio_play = function() assert(currentAsset and gain == 0); musicPlaying = true end,
    audio_stop = function() musicPlaying = false end,
}
local host = {
    find = function(_, id) return ({["room-music"] = playlist, ["opening-story-music"] = source})[id] end,
}
local music = new_story_music(host, {NEWings = "assets:/music/first.ogg", Nostalgia = "assets:/music/tutorial.ogg"})
local function close(a, b) return math.abs(a - b) <= 1e-6 end

music:play({"NEWings", "0.6", "0", "0"})
assert(not playlistPlaying and musicPlaying and currentAsset == "assets:/music/first.ogg")
music:update(.75)
assert(close(gain, .3))
music:update(.75)
assert(close(gain, .6))
music:finish()
music:update(.75)
assert(close(gain, .3) and musicPlaying and not playlistPlaying)
music:update(.75)
assert(gain == 0 and not musicPlaying and playlistPlaying and playlist:audio_position() == 37)
assert(#transitions == 2 and transitions[1] and not transitions[2])

playlistPlaying = false
music:play({"Nostalgia", "0", "0", "0"})
music:update(1.5)
assert(gain == 1 and #transitions == 2 and currentAsset == "assets:/music/tutorial.ogg")
music:finish()
music:update(1.5)
assert(not playlistPlaying and #transitions == 2)
