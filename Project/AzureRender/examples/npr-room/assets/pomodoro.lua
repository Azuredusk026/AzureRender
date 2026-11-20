local slot = "pomodoro-settings"
local defaults = {25, 5, 2}
local limits = {999, 999, 99}
local controls = {"work", "break", "rounds"}
local settings = defaults
local maxCountupSeconds = 359999
local countupMode = false
local countupElapsed = 0
local countupSinceSave = 0
local lastCountup = 0
local pendingCountup
local countupSaveRetry = 0

local phase = "idle"
local round = 0
local remaining = defaults[1] * 60
local paused = false
local previousPhase
local previousTime
local previousSettings
local workCalendar
local statsOpen
local heroineParts = {"face", "body", "hair", "top", "bottom", "socks", "jacket"}
local roomVoice
local workChoice, speaking
local choiceSpeaking
local roomLanguage
local timerVoice
local roomOpening, timerVoiceActive
local roomAchievements, timerAchievementSaved

local function durationText(value)
    local seconds = math.floor(value + 1e-7)
    return string.format("%02d:%02d:%02d", seconds // 3600, seconds // 60 % 60, seconds % 60)
end

local function playCue(id)
    local cue = self:find(id)
    if cue then
        cue:try_audio_play()
    end
end

local function setHeroineState(state)
    for _, part in ipairs(heroineParts) do
        local actor = self:find("heroine-" .. part)
        if actor then actor:set("azure.animator", "state", state) end
    end
end

local function announceTimer(event, fresh, midway)
    if timerVoice then
        local state = phase == "work" and not paused and "report" or "idle"
        setHeroineState(state)
        timerVoice:trigger(event, countupMode, round, settings[1] * settings[3] * 60, fresh, midway, state,
                           speaking or choiceSpeaking)
    end
end

local function updateDisplay()
    local label = paused and "paused" or phase
    local text = countupMode and "Count-up"
        or string.format("%dm work · %dm break · %d rounds", settings[1], settings[2], settings[3])
    if roomLanguage then label, text = roomLanguage:timer(phase, paused, countupMode, settings) end
    local seconds = math.max(0, math.ceil(remaining - 1e-7))
    local time = string.format("%02d:%02d", seconds // 60, seconds % 60)
    if countupMode then
        time = durationText(countupElapsed)
    end
    if label ~= previousPhase then
        self:ui_text("timer-phase", label)
        previousPhase = label
    end
    if time ~= previousTime then
        self:ui_text("timer-value", time)
        previousTime = time
    end
    if text ~= previousSettings then
        self:ui_text("timer-settings", text)
        previousSettings = text
    end
end

local function idle()
    phase = "idle"
    round = 0
    remaining = settings[1] * 60
    paused = false
    countupElapsed = 0
    countupSinceSave = 0
end

local function syncTimerAchievement()
    if roomAchievements and not timerAchievementSaved and (not workCalendar or workCalendar:completed_timer()) then
        timerAchievementSaved = roomAchievements:once("COMPLETE_POMODORO_COUNT")
    end
end

local function finish(midway)
    if workCalendar then workCalendar:settle(roomAchievements ~= nil) end
    syncTimerAchievement()
    if workChoice then workChoice:complete() end
    idle()
    setHeroineState("idle")
    playCue("timer-finish")
    announceTimer("finish", false, midway)
end

local function finishInterval()
    if round == settings[3] then
        finish()
    elseif phase == "work" then
        phase = "break"
        remaining = settings[2] * 60
        playCue("timer-finish")
        announceTimer("break")
    else
        phase = "work"
        round = round + 1
        remaining = settings[1] * 60
        playCue("timer-start")
        announceTimer("work")
    end
end

local function loadSettings()
    local saved = self:read_save(slot)
    if type(saved) ~= "table" or #saved ~= 4 or saved[1] ~= 1 then
        return defaults
    end
    local values = {}
    for index, upper in ipairs(limits) do
        local value = saved[index + 1]
        if type(value) ~= "number" or value % 1 ~= 0 or value < 1 or value > upper then
            return defaults
        end
        values[index] = value
    end
    return values
end

local function completedSeconds(value)
    if type(value) == "number" and value >= 0 and value <= maxCountupSeconds and value % 1 == 0 then
        return value
    end
    return 0
end

local function tryWriteTimerRecord(name, value)
    local stored = self:read_save(name)
    if type(stored) == "table" and type(stored[1]) == "number" and stored[1] > value[1] then
        return false
    end
    return self:try_write_save(name, value)
end

local function loadCountup()
    local mode = self:read_save("timer-mode")
    countupMode = type(mode) == "table" and #mode == 2 and mode[1] == 1 and mode[2] == 1
    lastCountup = 0
    pendingCountup = nil
    countupSaveRetry = 0
    local session = self:read_save("countup-session")
    if type(session) == "table" and #session == 3 and session[1] == 1 then
        local interrupted = completedSeconds(session[2])
        lastCountup = completedSeconds(session[3])
        if interrupted > 0 then
            lastCountup = interrupted
            pendingCountup = {1, 0, lastCountup}
        end
    end
end

local function publishCountup()
    if not pendingCountup then return end
    if tryWriteTimerRecord("countup-session", pendingCountup) then
        pendingCountup = nil
    else
        countupSaveRetry = 1
    end
end

local function checkpointCountup(active)
    local seconds = math.min(maxCountupSeconds, math.floor(countupElapsed + 1e-7))
    if not active then lastCountup = seconds end
    pendingCountup = {1, active and seconds or 0, lastCountup}
    publishCountup()
end

local function finishCountup()
    checkpointCountup(false)
    finish()
end

local function updateCountup(dt)
    if self:pressed("timer-reset") and phase ~= "idle" then
        finishCountup()
    elseif self:pressed("timer-toggle") then
        if phase == "idle" then
            phase = "work"
            setHeroineState("report")
            playCue("timer-start")
            announceTimer("work", true)
        else
            paused = not paused
            countupSinceSave = 0
            if paused and countupElapsed >= 1 then checkpointCountup(true) end
            playCue(paused and "timer-finish" or "timer-start")
            announceTimer(paused and "break" or "work")
        end
    elseif phase == "work" and not paused then
        countupElapsed = countupElapsed + dt
        countupSinceSave = countupSinceSave + dt
        if math.floor(countupElapsed + 1e-7) > maxCountupSeconds then
            finishCountup()
        elseif countupSinceSave >= 60 then
            checkpointCountup(true)
            countupSinceSave = 0
        end
    end
end

local function adjustSettings()
    for index, name in ipairs(controls) do
        local prefix = "timer-" .. name
        local direction = self:pressed(prefix .. "-up") and 1 or self:pressed(prefix .. "-down") and -1 or 0
        if direction ~= 0 then
            local nextValue = math.max(1, math.min(limits[index], settings[index] + direction))
            if nextValue ~= settings[index] then
                local updated = {settings[1], settings[2], settings[3]}
                updated[index] = nextValue
                if not tryWriteTimerRecord(slot, {1, updated[1], updated[2], updated[3]}) then return end
                settings = updated
                remaining = settings[1] * 60
            end
            playCue(direction > 0 and "settings-click" or "settings-cancel")
            return
        end
    end
end

function init()
    settings = loadSettings()
    loadCountup()
    idle()
    roomAchievements = new_room_achievements and new_room_achievements(self)
    timerAchievementSaved = false
    local translate
    if room_language_config then
        roomLanguage = new_room_language(room_language_config)
        translate = function(value) return roomLanguage:text(value) end
    end
    if new_room_voice then roomVoice = new_room_voice(translate) end
    if new_work_choice then workChoice = new_work_choice(nil, translate) end
    if timer_voice_program then
        timerVoice = new_timer_voice_action(timer_voice_program, setHeroineState,
                                            translate or function(value) return value end)
    end
    if new_work_calendar then
        workCalendar = new_work_calendar(translate)
        syncTimerAchievement()
        statsOpen = false
        self:ui_visible("timer-stats", true)
        self:ui_visible("work-stats", false)
        self:ui_visible("work-progression", workCalendar:progress() ~= nil)
    end
    updateDisplay()
    if new_room_opening then
        roomOpening = new_room_opening(roomLanguage, function()
            local progress = workCalendar and workCalendar:progress()
            return progress and progress[1] or 1
        end)
    end
end

function update(dt)
    if pendingCountup then
        countupSaveRetry = countupSaveRetry - dt
        if countupSaveRetry <= 0 then publishCountup() end
    end
    local languageChanged = roomLanguage and roomLanguage:update()
    if workCalendar then workCalendar:recover() end
    if roomOpening and roomOpening:update(dt, phase == "idle" and not speaking and not choiceSpeaking and not timerVoiceActive,
                                         languageChanged) then return end
    local voicedWork = 0
    if (workCalendar or workChoice or timerVoice) and phase ~= "idle" then
        local working = phase == "work" and not paused and not self:pressed("timer-toggle")
            and not self:pressed("timer-reset") and (countupMode or not self:pressed("timer-skip"))
        local available = countupMode and maxCountupSeconds - countupElapsed or remaining
        local spent = math.min(dt, available)
        if workCalendar then workCalendar:advance(spent, working) end
        if workChoice and working then workChoice:recordWork(spent) end
        if working then voicedWork = spent end
    end
    if timerVoice then timerVoice:advanceClock(dt, voicedWork) end
    if phase == "idle" and self:pressed("timer-mode") then
        local nextMode = not countupMode
        if tryWriteTimerRecord("timer-mode", {1, nextMode and 1 or 0}) then
            countupMode = nextMode
            playCue("settings-click")
        end
    elseif countupMode then
        updateCountup(dt)
    elseif self:pressed("timer-reset") and phase ~= "idle" then
        finish(true)
    elseif self:pressed("timer-toggle") then
        if phase == "idle" then
            phase = "work"
            round = 1
            setHeroineState("report")
            playCue("timer-start")
            announceTimer("work", true)
        else
            paused = not paused
            playCue(paused and "timer-pause" or "timer-start")
        end
    elseif phase ~= "idle" then
        if self:pressed("timer-skip") then
            remaining = 0
        elseif not paused then
            remaining = math.max(0, remaining - dt)
        end
        if not paused and remaining <= 1e-7 then
            finishInterval()
        end
    else
        adjustSettings()
    end
    updateDisplay()
    if workCalendar then
        if self:pressed("timer-stats") and (not statsOpen or workCalendar:close_month_view()) then
            statsOpen = not statsOpen
            self:ui_visible("work-stats", statsOpen)
        end
        if statsOpen then
            self:ui_text("work-today", durationText(workCalendar:today()))
            self:ui_text("work-total", durationText(workCalendar:totalTime()))
            local date, time = workCalendar:update_month_view(languageChanged or self:pressed("timer-stats"))
            self:ui_text("calendar-selected-date", date)
            self:ui_text("calendar-selected-time", durationText(time))
            local growth = workCalendar:progress()
            if growth then
                self:ui_text("work-progression", string.format(" · Level %d · Points %d", growth[1], growth[3]))
            end
        end
    end
    timerVoiceActive = timerVoice and timerVoice:update(dt, languageChanged)
    choiceSpeaking = workChoice and workChoice:step(dt, phase == "idle" and not speaking and not timerVoiceActive,
                                                      languageChanged)
    if roomVoice then speaking = roomVoice(dt, phase == "idle", choiceSpeaking or timerVoiceActive,
                                           languageChanged) end
end

function shutdown()
    if countupMode and phase == "work" and countupElapsed >= 1 then
        checkpointCountup(true)
    else
        publishCountup()
    end
    if workCalendar then
        workCalendar:save_open_diary()
        workCalendar:save_open_work_time()
        workCalendar:flush()
    end
end
