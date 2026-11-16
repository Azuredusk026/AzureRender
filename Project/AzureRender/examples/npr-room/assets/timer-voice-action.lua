function new_timer_voice_action(program, setMotion, translate)
    local host = self
    local audio = assert(host:find("room-timer-voice"))
    local bank = new_timer_voice_bank(program.groups, program.random)
    local now, previousWorkEnd, intervalWorked, totalWorked = 0, nil, 0, 0
    local action, controller = nil, {}

    local function showLine()
        host:ui_text("timer-voice-line", translate(action.line.text))
        host:ui_visible("timer-voice-caption", true)
    end

    local function startVoice()
        audio:set("azure.audio-source", "asset", action.line.asset)
        audio:audio_play()
        action.voice = "starting"
        action.voiceElapsed = 0
        showLine()
    end

    local function startMotion()
        setMotion(action.motion.state)
        action.motionElapsed = 0
    end

    function controller:advanceClock(dt, work)
        now = now + dt
        intervalWorked = intervalWorked + work
        totalWorked = totalWorked + work
    end

    function controller:trigger(event, countup, round, planned, fresh, midway, restore, suppress)
        local scenario = timer_voice_scenario(program.rules, event, countup, {
            pomodoroRunning = not countup and event ~= "finish", round = round, planned = planned,
            previousWorkEnd = previousWorkEnd, now = now, intervalWorked = intervalWorked,
            totalWorked = totalWorked, midway = midway,
        }, program.random)
        if event ~= "work" then previousWorkEnd = now end
        if event == "work" then intervalWorked = 0 end
        if fresh then totalWorked = 0 end
        if action then
            if action.voice == "starting" or action.voice == "playing" then audio:audio_stop() end
            host:ui_visible("timer-voice-caption", false)
            setMotion(restore)
        end
        action = nil
        if suppress then return false end
        local line = bank:take(scenario)
        local motion = program.motions[line.bodyMotion]
        if line.bodyMotion >= 0 and not motion then return false end
        local cue = motion and not line.speakBeforeMotion and assert(motion.cues[line.animationCue])
        action = {line = line, motion = motion, cue = cue, restore = restore}
        if line.speakBeforeMotion or not motion then
            startVoice()
        else
            startMotion()
            if cue == 0 then startVoice() end
        end
        return true
    end

    function controller:update(dt, languageChanged)
        if not action then return false end
        if action.voice == "starting" then
            -- The host applies the queued source changes after this callback.
            action.voice = "playing"
        elseif action.voice == "playing" then
            if languageChanged then showLine() end
            action.voiceElapsed = action.voiceElapsed + dt
            local complete = not audio:audio_playing() and audio:audio_position() >= action.line.seconds
            if complete or action.voiceElapsed >= 30 then
                if not complete then audio:audio_stop() end
                action.voice = "finished"
                host:ui_visible("timer-voice-caption", false)
                if action.line.speakBeforeMotion and action.motion then startMotion() end
            end
        end
        if action.motionElapsed then
            action.motionElapsed = action.motionElapsed + dt
            if action.cue and not action.voice and action.motionElapsed >= action.cue then startVoice() end
            if action.motionElapsed > action.motion.seconds then
                setMotion(action.restore)
                action.motionElapsed, action.motionComplete = nil, true
            end
        end
        if action.voice == "finished" and (not action.motion or action.motionComplete) then action = nil end
        return action ~= nil
    end

    return controller
end
