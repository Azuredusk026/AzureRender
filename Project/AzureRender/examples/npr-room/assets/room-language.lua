function new_room_language(config)
    local host = self
    local slot = "room-language"
    local selected = config.default
    local saved = host:read_save(slot)
    local open = false
    local lastToggle, lastMode
    local language = {}

    if type(saved) == "table" and #saved == 2 and saved[1] == 1 and config.strings.language[saved[2]] then
        selected = saved[2]
    end

    function language:text(value)
        if type(value) == "table" then return assert(value[selected]) end
        return value
    end

    local function label(key)
        return language:text(config.strings[key])
    end

    local function updateButtons()
        host:ui_text("room-language-toggle", label("language"))
        host:ui_text("timer-end-button", label("finish") .. " · R")
        host:ui_text("timer-stats", label("statistics"))
    end

    function language:update()
        if host:pressed("room-language-toggle") then
            open = not open
            host:ui_visible("room-language-panel", open)
        end
        if not open then return false end
        for index in ipairs(config.strings.language) do
            if host:pressed("room-language-" .. index) then
                if index == selected then return false end
                local current = host:read_save(slot)
                local future = type(current) == "table" and type(current[1]) == "number" and current[1] > 1
                if future or not host:try_write_save(slot, {1, index}) then return false end
                selected = index
                updateButtons()
                return true
            end
        end
        return false
    end

    function language:timer(phase, paused, countup, values)
        local mode = label(countup and "countup" or "pomodoro")
        local switchMode = label(countup and "pomodoro" or "countup") .. " · M"
        local toggle = label(phase == "idle" and "start" or paused and "resume" or "pause") .. " · T"
        if switchMode ~= lastMode then
            host:ui_text("timer-mode-button", switchMode)
            lastMode = switchMode
        end
        if toggle ~= lastToggle then
            host:ui_text("timer-toggle-button", toggle)
            lastToggle = toggle
        end
        local title = paused and label("pause") or phase == "work" and label("working")
            or phase == "break" and label("resting") or mode
        local settings = countup and "" or string.format("%s %d min · %s %d min · %s %d",
            label("work"), values[1], label("rest"), values[2], label("loop"), values[3])
        return title, settings
    end

    updateButtons()
    host:ui_visible("room-language-panel", false)
    return language
end
