function new_room_story(driver, progress)
    local story = {}
    local phase = "idle"
    local chapter, replay, position, row, auto, remaining, choice, used, ready
    local effectDelay = 0
    local choices = {Selection = true, TutorialSelection = true}
    local delayed = {DirectionMonochromeNoise = true, DirectionChromaticAberration = true,
                     DirectionGlitch1 = true, DirectionScreenMove = true, DirectionBlackOut = true,
                     DirectionGameStartCamera = true}

    local function seek(index)
        position, phase = index, "read"
    end

    local function finish(completed)
        if completed and not replay and not progress:complete(chapter) then
            phase = "persist"
            return
        end
        driver:finish(chapter, replay, completed)
        phase, chapter = "idle", nil
    end

    local function displayChoices(command)
        choice = {}
        local index = position
        while chapter.commands[index] and chapter.commands[index].command == command do
            choice[#choice + 1] = chapter.commands[index]
            index = index + 1
        end
        driver:clear()
        driver:choices(choice, used)
        phase = "choice"
    end

    local function step()
        row = chapter.commands[position]
        if not row or row.command == "EndScenario" then finish(true); return end
        local command = row.command
        if command == "Text" then
            driver:motion(row)
            remaining, phase = row.delay, "delay"
        elseif command == "WaitForSeconds" then
            remaining, phase = row.seconds, "wait"
        elseif command == "ReadNextForScript" then
            driver:clear()
            phase = "script"
        elseif choices[command] then
            displayChoices(command)
        elseif command == "Jump" then
            driver:clear()
            seek(row.target)
        else
            if command == "SetDirectionDelaySeconds" then
                effectDelay = assert(tonumber(row.arguments[1]))
            elseif command == "StartSelectOnce" then
                used = {}
            elseif command == "DeactivateNormalText" then
                driver:clear()
            elseif command == "ChangeMotion" then
                driver:motion(row)
            elseif command ~= "TalkLogOnlyPlayBGM" or replay then
                ready = driver:effect(row, delayed[command] and effectDelay or 0)
                if delayed[command] then effectDelay = 0 end
            end
            if ready then phase = "effect" else seek(position + 1) end
        end
    end

    function story:state() return phase end

    function story:start(selected, level, log)
        if phase ~= "idle" or not progress:canStart(selected, level, log) then return false end
        chapter, replay, auto, used, ready, effectDelay = selected, log, false, {}, nil, 0
        seek(1)
        driver:begin(chapter, replay)
        return true
    end

    function story:cancel()
        if chapter and phase ~= "persist" then finish(false) end
    end

    function story:update(dt, input)
        if not chapter then return end
        if input == "auto" then auto = not auto end
        if phase == "persist" and input == "next" then finish(true); return end
        if phase == "script" and input == "script" then seek(position + 1) end
        if phase == "choice" and type(input) == "number" then
            local selected = choice[input]
            if selected then
                if selected.usedKey then used[selected.usedKey] = true end
                driver:clear()
                seek(selected.target)
            end
        elseif phase == "effect" and ready() then
            ready = nil
            seek(position + 1)
        elseif phase == "text" then
            if driver:textReady() then
                local nextRow = chapter.commands[position + 1]
                if input == "next" or (nextRow and nextRow.command == "Selection") then
                    seek(position + 1)
                else
                    remaining = math.max(0, remaining - dt)
                    if auto and remaining <= 1e-7 then seek(position + 1) end
                end
            elseif input == "next" then
                driver:reveal()
            end
        end
        if phase == "delay" or phase == "wait" then
            remaining = remaining - dt
            if remaining > 1e-7 then return end
            if phase == "delay" then
                driver:text(row)
                remaining, phase = row.hold, "text"
                return
            end
            seek(position + 1)
        end
        local budget = #chapter.commands + 1
        while phase == "read" and budget > 0 do
            budget = budget - 1
            step()
            if phase == "delay" and remaining == 0 then
                driver:text(row)
                remaining, phase = row.hold, "text"
            end
        end
        assert(phase ~= "read", "Story command loop requires a wait or input")
    end

    return story
end
