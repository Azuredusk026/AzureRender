function new_timer_sound_menu(host, settings, translate)
    local open, countup = false, false
    local initialized
    local groups = {"start", "finish"}
    local choiceCount = 3
    local menu = {}

    local function label(name)
        return translate(timer_sound_labels[name])
    end

    local function showLabels()
        host:ui_text("timer-sound-toggle", label("title"))
        host:ui_text("timer-sound-pomodoro-label", label("pomodoro"))
        host:ui_text("timer-sound-countup-label", label("countup"))
        host:ui_text("timer-sound-start-label", label("start"))
        host:ui_text("timer-sound-finish-label", label("finish"))
    end

    local function redraw()
        host:ui_visible("timer-sound-pomodoro-active", not countup)
        host:ui_visible("timer-sound-countup-active", countup)
        for group, name in ipairs(groups) do
            local choice, volume, mute = settings:values(countup, group)
            local prefix = "timer-sound-" .. name
            for option = 1, choiceCount do
                host:ui_visible(prefix .. "-" .. option .. "-active", choice == option)
            end
            host:ui_set_value(prefix .. "-volume", tostring(volume))
            host:ui_text(prefix .. "-mute", label(mute == 1 and "off" or "on"))
        end
    end

    local function change(group, field, value, preview)
        local ok = settings:change(countup, group, field, value, preview)
        host:ui_text("timer-sound-status", ok and "" or label("error"))
        redraw()
    end

    function menu:update(activeMode, languageChanged)
        if open then
            for group, name in ipairs(groups) do
                local field = "timer-sound-" .. name .. "-volume"
                if host:pressed(field) then
                    change(group, 2, tonumber(host:ui_value(field)), true)
                end
            end
        end
        if not initialized or languageChanged then
            showLabels()
            if open then redraw() end
            initialized = true
        end
        if host:pressed("timer-sound-toggle") then
            open = not open
            host:ui_visible("timer-sound-panel", open)
            host:ui_text("timer-sound-status", "")
            local cue = host:find(open and "settings-click" or "settings-cancel")
            if cue then cue:try_audio_play() end
            if open then
                countup = activeMode
                redraw()
            end
            return
        end
        if not open then return end
        local pomodoroTab = host:pressed("timer-sound-pomodoro")
        local countupTab = host:pressed("timer-sound-countup")
        if pomodoroTab or countupTab then
            countup = countupTab and not pomodoroTab
            host:ui_text("timer-sound-status", "")
            local cue = host:find("settings-click")
            if cue then cue:try_audio_play() end
            redraw()
            return
        end
        for group, name in ipairs(groups) do
            local prefix = "timer-sound-" .. name
            for choice = 1, choiceCount do
                if host:pressed(prefix .. "-" .. choice) then
                    change(group, 1, choice, true)
                    return
                end
            end
            if host:pressed(prefix .. "-mute") then
                local _, _, mute = settings:values(countup, group)
                change(group, 3, 1 - mute, false)
                return
            end
        end
    end

    host:ui_visible("timer-sound-panel", false)
    return menu
end
