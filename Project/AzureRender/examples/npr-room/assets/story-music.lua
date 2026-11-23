function new_story_music(host, tracks)
    local playlist = assert(host:find("room-music"))
    local source = assert(host:find("opening-story-music"))
    local music = {}
    local gain, fade, active, resumePlaylist = 0, nil, false, false

    local function volume(value)
        gain = value
        source:set("azure.audio-source", "volume", value)
    end

    local function transition(to, seconds)
        fade = {from = gain, to = to, duration = seconds, elapsed = 0}
    end

    function music:play(arguments)
        local asset = assert(tracks[arguments[1]])
        local ratio, offset, seconds = tonumber(arguments[2]), tonumber(arguments[3]), tonumber(arguments[4])
        assert(offset == 0 and seconds >= 0)
        if active then
            source:audio_stop()
        else
            resumePlaylist = playlist:audio_playing()
            if resumePlaylist then playlist:audio_pause(true) end
        end
        active = true
        source:set("azure.audio-source", "asset", asset)
        volume(0)
        if not source:try_audio_play() then
            if resumePlaylist then playlist:audio_pause(false) end
            active, resumePlaylist, fade = false, false, nil
            return false
        end
        transition(ratio == 0 and 1 or math.max(0, math.min(1, ratio)), seconds == 0 and 1.5 or seconds)
        return true
    end

    function music:finish()
        if active then transition(0, 1.5) end
    end

    function music:update(dt)
        if not fade then return end
        fade.elapsed = math.min(fade.duration, fade.elapsed + dt)
        local progress = fade.elapsed / fade.duration
        volume(fade.from + (fade.to - fade.from) * progress)
        if progress < 1 then return end
        local finished = fade.to == 0
        fade = nil
        if finished then
            source:audio_stop()
            if resumePlaylist then playlist:audio_pause(false) end
            active, resumePlaylist = false, false
        end
    end

    return music
end
