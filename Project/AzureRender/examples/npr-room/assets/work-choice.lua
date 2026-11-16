function new_work_choice(random, translate)
    random = random or math.random
    translate = translate or function(text) return text end
    local timer = self
    local voice = assert(timer:find("room-choice-voice"))
    local slot = "work-conversation"
    local intro, options, response = 1, 2, 3
    local worked, available, probability = 0, 0, 0
    local episode, stage, selected = 0, 0, 0
    local lineup, last = {}, nil
    local line, delay, hold
    local controller = {}

    local function checkpoint()
        local ok = timer:try_write_save(slot, {1, episode, stage, selected})
        timer:ui_text("room-choice-status", ok and "" or "Conversation progress could not be saved")
    end

    local function playLine(value)
        line, delay, hold = value, value[4], nil
        timer:ui_visible("room-choice-options", false)
        timer:ui_visible("room-choice-caption", false)
    end

    local function showOptions()
        local story = work_conversations[episode]
        timer:ui_text("room-choice-question", translate(story[1][2]))
        for index, option in ipairs(story[2]) do timer:ui_text("room-choice-option-" .. index, translate(option[1])) end
        timer:ui_visible("room-choice-caption", false)
        timer:ui_visible("room-choice-options", true)
    end

    local function nextEpisode()
        if #lineup == 0 then
            for index = 1, #work_conversations do lineup[index] = index end
            for index = #lineup, 2, -1 do
                local other = random(index)
                lineup[index], lineup[other] = lineup[other], lineup[index]
            end
            if #lineup > 1 and lineup[#lineup] == last then
                lineup[1], lineup[#lineup] = lineup[#lineup], lineup[1]
            end
        end
        last = table.remove(lineup)
        return last
    end

    local saved = timer:read_save(slot)
    if type(saved) == "table" and saved[1] == 1 and work_conversations[saved[2]] then
        local savedStage, savedOption = saved[3], saved[4]
        local story = work_conversations[saved[2]]
        if savedStage == options or savedStage == intro or (savedStage == response and story[2][savedOption]) then
            episode, stage = saved[2], savedStage
            if stage == options then showOptions()
            elseif stage == intro then playLine(story[1])
            else selected = savedOption; playLine(story[2][selected][2]) end
        end
    end

    function controller:recordWork(seconds) worked = worked + seconds end

    function controller:complete()
        probability = math.min(1, worked / work_choice_parameters.fullProbabilitySeconds)
            * work_choice_parameters.probability
        available, worked = work_choice_parameters.availableSeconds, 0
    end

    function controller:step(dt, mayStart, languageChanged)
        available = math.max(0, available - dt)
        if episode == 0 then
            if not mayStart or not timer:pressed("room-voice-talk") or available <= 0 or probability <= 0
                or random() > probability then return false end
            available, episode, stage = 0, nextEpisode(), intro
            checkpoint()
            playLine(work_conversations[episode][1])
        end

        if stage == options then
            if languageChanged then showOptions() end
            for index, option in ipairs(work_conversations[episode][2]) do
                if timer:pressed("room-choice-option-" .. index) then
                    stage, selected = response, index
                    checkpoint()
                    playLine(option[2])
                    break
                end
            end
        elseif delay then
            delay = delay - dt
            if delay <= 0 then
                delay = nil
                voice:set("azure.audio-source", "asset", line[1])
                voice:audio_play()
                timer:ui_text("room-choice-line", translate(line[2]))
                timer:ui_visible("room-choice-caption", true)
            end
        elseif hold then
            hold = hold - dt
            if hold <= 0 then
                if stage == intro then
                    stage = options
                    checkpoint()
                    showOptions()
                else
                    episode, stage, selected = 0, 0, 0
                    checkpoint()
                    timer:ui_visible("room-choice-caption", false)
                end
            end
        elseif not voice:audio_playing() and voice:audio_position() >= line[3] then
            hold = line[5]
        end
        if languageChanged and stage ~= options and episode ~= 0 and not delay then
            timer:ui_text("room-choice-line", translate(line[2]))
        end
        return true
    end

    return controller
end
