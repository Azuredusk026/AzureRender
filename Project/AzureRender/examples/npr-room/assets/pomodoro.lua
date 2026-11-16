local slot = "pomodoro-settings"
local defaults = {25, 5, 2}
local limits = {999, 999, 99}
local controls = {"work", "break", "rounds"}
local settings = defaults

local phase = "idle"
local round = 0
local remaining = defaults[1] * 60
local paused = false
local previousPhase
local previousTime
local previousSettings

local function playCue(id)
    local cue = self:find(id)
    if cue then
        cue:audio_play()
    end
end

local function updateDisplay()
    local label = paused and "paused" or phase
    local seconds = math.max(0, math.ceil(remaining - 1e-7))
    local time = string.format("%02d:%02d", seconds // 60, seconds % 60)
    if label ~= previousPhase then
        self:ui_text("timer-phase", label)
        previousPhase = label
    end
    if time ~= previousTime then
        self:ui_text("timer-value", time)
        previousTime = time
    end
    local text = string.format("%dm work · %dm break · %d rounds", settings[1], settings[2], settings[3])
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
end

local function finish()
    idle()
    playCue("timer-finish")
end

local function finishInterval()
    if round == settings[3] then
        finish()
    elseif phase == "work" then
        phase = "break"
        remaining = settings[2] * 60
        playCue("timer-finish")
    else
        phase = "work"
        round = round + 1
        remaining = settings[1] * 60
        playCue("timer-start")
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

local function adjustSettings()
    for index, name in ipairs(controls) do
        local prefix = "timer-" .. name
        local direction = self:pressed(prefix .. "-up") and 1 or self:pressed(prefix .. "-down") and -1 or 0
        if direction ~= 0 then
            local nextValue = math.max(1, math.min(limits[index], settings[index] + direction))
            if nextValue ~= settings[index] then
                local updated = {settings[1], settings[2], settings[3]}
                updated[index] = nextValue
                self:write_save(slot, {1, updated[1], updated[2], updated[3]})
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
    idle()
    updateDisplay()
end

function update(dt)
    if self:pressed("timer-reset") and phase ~= "idle" then
        finish()
    elseif self:pressed("timer-toggle") then
        if phase == "idle" then
            phase = "work"
            round = 1
            playCue("timer-start")
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
end
