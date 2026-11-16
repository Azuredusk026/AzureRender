local workSeconds = 25 * 60
local breakSeconds = 5 * 60
local rounds = 2

local phase = "idle"
local round = 0
local remaining = workSeconds
local paused = false
local previousPhase
local previousTime

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
end

local function finish()
    phase = "idle"
    round = 0
    remaining = workSeconds
    paused = false
    playCue("timer-finish")
end

local function finishInterval()
    if round == rounds then
        finish()
    elseif phase == "work" then
        phase = "break"
        remaining = breakSeconds
        playCue("timer-finish")
    else
        phase = "work"
        round = round + 1
        remaining = workSeconds
        playCue("timer-start")
    end
end

function init()
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
    end
    updateDisplay()
end
