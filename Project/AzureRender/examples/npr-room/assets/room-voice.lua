function new_room_voice()
    local timer = self
    local source = assert(timer:find("room-voice"))
    local remaining = {}
    local current, last, delay, hold
    local visible = true

    local function selectResponse()
        if #remaining == 0 then
            for index = 1, #responses do remaining[index] = index end
            for index = #remaining, 2, -1 do
                local other = math.random(index)
                remaining[index], remaining[other] = remaining[other], remaining[index]
            end
            if #remaining > 1 and remaining[#remaining] == last then
                remaining[1], remaining[#remaining] = remaining[#remaining], remaining[1]
            end
        end
        last = table.remove(remaining)
        current = responses[last]
        delay = current[4]
    end

    local function start()
        source:set("azure.audio-source", "asset", current[1])
        source:audio_play()
        timer:ui_text("room-voice-line", current[2])
        timer:ui_visible("room-voice-caption", true)
    end

    return function(dt, idle)
        if idle ~= visible then
            visible = idle
            timer:ui_visible("room-voice-talk", visible)
        end
        if not current and idle and timer:pressed("room-voice-talk") then selectResponse() end
        if not current then return end
        if delay then
            delay = delay - dt
            if delay <= 0 then delay = nil; start() end
        elseif hold then
            hold = hold - dt
            if hold <= 0 then
                current, hold = nil, nil
                timer:ui_visible("room-voice-caption", false)
            end
        elseif not source:audio_playing() and source:audio_position() >= current[3] then
            hold = current[5]
        end
    end
end
