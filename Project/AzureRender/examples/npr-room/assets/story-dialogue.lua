function new_story_dialogue(host, speeds, currentLanguage)
    local audio = assert(host:find("opening-story-voice"))
    local dialogue = {}
    local line, caption, starts, length, shown, elapsed, speed, fast, voiceRemaining, pending

    local function publish()
        local boundary = starts[shown + 1]
        host:ui_text("opening-story-line", boundary and caption:sub(1, boundary - 1) or caption)
    end

    function dialogue:clear()
        if voiceRemaining then audio:audio_stop() end
        line, voiceRemaining, pending = nil, nil, false
        host:ui_visible("opening-story-caption", false)
    end

    function dialogue:languageChanged()
        if not line then return end
        local language = currentLanguage()
        local complete = length and shown > length
        caption, speed = assert(line.translations[language]), assert(speeds[language])
        starts = {}
        -- Localization is validated UTF-8. Continuation bytes stay with their first byte.
        for index = 1, #caption do
            local byte = caption:byte(index)
            if byte < 0x80 or byte >= 0xc0 then starts[#starts + 1] = index end
        end
        length = #starts
        shown = complete and length + 1 or math.min(shown, length + 1)
        publish()
    end

    function dialogue:show(value)
        self:clear()
        line, shown, elapsed, length, fast = value, 1, 0, nil, false
        self:languageChanged()
        host:ui_visible("opening-story-caption", true)
        if line.asset then
            voiceRemaining, pending = line.frames / line.sampleRate, true
            audio:set("azure.audio-source", "asset", "assets:/" .. line.asset)
            audio:audio_play()
        end
    end

    function dialogue:fastForward()
        fast = true
    end

    function dialogue:speaking()
        return line and line.moveMouth and audio:audio_playing()
    end

    function dialogue:update(dt)
        if not line then return end
        if pending then pending = false
        elseif voiceRemaining and not audio:audio_playing() and audio:audio_position() >= voiceRemaining then
            voiceRemaining = nil
        end
        if shown > length then return end
        local step = fast and .01 or speed
        elapsed = elapsed + dt
        local count = math.floor((elapsed + 1e-7) / step)
        if count == 0 then return end
        elapsed = math.max(0, elapsed - count * step)
        shown = math.min(length + 1, shown + count)
        publish()
    end

    function dialogue:ready()
        return line ~= nil and shown > length and not voiceRemaining
    end

    return dialogue
end
