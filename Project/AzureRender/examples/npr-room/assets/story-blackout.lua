function new_story_hold(command, apply, readsBlind, playback)
    local effect = {}
    local time, pending, prepared = 0, {}, {}
    local ratio, blind = 0, false
    local firstRun = {Text = true, ChangeMotion = true, AddTextLoop = true,
        DirectionGameStartCamera = true, WaitForSeconds = true}

    local function values(row)
        local amount = assert(tonumber(row.arguments[1]))
        local hold = assert(tonumber(row.arguments[2]))
        local useBlind = row.arguments[3]
        assert(amount >= 0 and amount <= 1 and (hold == -1 or hold >= 0))
        return amount, hold, readsBlind and useBlind ~= nil and useBlind ~= "" and tonumber(useBlind) == 1
    end

    local function publish(amount, useBlind)
        if amount == ratio and useBlind == blind then return end
        ratio, blind = amount, useBlind
        apply(ratio, blind, time)
    end

    local function queue(at, amount, useBlind, started)
        local index = #pending + 1
        while index > 1 and pending[index - 1].at > at do index = index - 1 end
        table.insert(pending, index, {at = at, ratio = amount, blind = useBlind, started = started})
    end

    function effect:update(dt)
        time = time + dt
        while pending[1] and pending[1].at <= time + 1e-9 do
            local next = table.remove(pending, 1)
            if playback then playback(next.started, time) end
            publish(next.ratio, next.blind)
        end
    end

    function effect:finish()
        time, pending, prepared = 0, {}, {}
        publish(0, false)
    end

    function effect:begin(chapter, replay)
        self:finish()
        if not replay and chapter.category ~= 1 then return end
        local amount, hold, useBlind = 0, -1, false
        for _, row in ipairs(chapter.commands) do
            if firstRun[row.command] then break end
            if row.command == command then
                prepared[row.row] = true
                amount, hold, useBlind = values(row)
            end
        end
        publish(hold == -1 and amount or 0, useBlind)
    end

    function effect:effect(row, delay)
        if row.command ~= command or prepared[row.row] then return end
        local amount, hold, useBlind = values(row)
        local start = time + delay
        queue(start, amount, useBlind, true)
        if hold >= 0 then queue(start + hold, 0, false, false) end
    end

    return effect
end

function new_story_blackout(apply)
    return new_story_hold("DirectionBlackOut", apply, true)
end
