function new_story_glitch(writeRatio, writeCamera, drawInterval)
    local now, deadline, cameraActive = 0, nil, false
    local effect = {}

    local function setCamera(enabled)
        if cameraActive == enabled then return end
        cameraActive = enabled
        writeCamera(enabled, now)
    end

    local function schedule()
        local seconds = drawInterval and drawInterval(.3, 1) or .3 + .7 * math.random()
        assert(seconds >= .3 and seconds <= 1)
        deadline = now + seconds
    end

    local held = new_story_hold("DirectionGlitch1", function(ratio, _, seconds)
        writeRatio(ratio, seconds)
    end, false, function(playing)
        setCamera(playing)
        if playing then schedule() else deadline = nil end
    end)

    function effect:update(dt)
        now = now + dt
        held:update(dt)
        if deadline and now + 1e-9 >= deadline then
            setCamera(not cameraActive)
            schedule()
        end
    end

    function effect:effect(row, delay)
        held:effect(row, delay)
    end

    function effect:finish()
        setCamera(false)
        now, deadline = 0, nil
        held:finish()
    end

    function effect:begin(chapter, replay)
        self:finish()
        held:begin(chapter, replay)
    end

    return effect
end
