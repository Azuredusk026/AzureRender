function new_story_ratio(command, write)
    local current, now, pending, tween, prepared = 0, 0, {}, nil, {}
    local effect = {}
    local firstRun = {Text = true, ChangeMotion = true, AddTextLoop = true,
                      DirectionGameStartCamera = true, WaitForSeconds = true}

    local function publish(value)
        if current == value then return end
        current = value
        write(current, now)
    end

    local function advance(target)
        now = target
        if tween then
            if target + 1e-9 >= tween.at + tween.seconds then
                publish(tween.to)
                tween = nil
            else
                local ratio = (target - tween.at) / tween.seconds
                if tween.outQuad then ratio = 1 - (1 - ratio) ^ 2 end
                publish(tween.from + ratio * (tween.to - tween.from))
            end
        end
    end

    function effect:update(dt)
        local target = now + dt
        while pending[1] and pending[1].at <= target do
            local next = table.remove(pending, 1)
            advance(next.at)
            if next.seconds == 0 then
                tween = nil
                publish(next.to)
            else
                next.from = current
                tween = next
            end
        end
        advance(target)
    end

    function effect:finish()
        pending, tween, prepared, now = {}, nil, {}, 0
        publish(0)
    end

    function effect:begin(chapter, replay)
        self:finish()
        if replay or chapter.category == 1 then
            local start = 0
            for _, row in ipairs(chapter.commands) do
                if firstRun[row.command] then break end
                if row.command == command then
                    prepared[row.row] = true
                    start = assert(tonumber(row.arguments[1]))
                end
            end
            publish(start)
        end
    end

    function effect:effect(row, delay)
        if row.command ~= command or prepared[row.row] then return end
        local amount, seconds, ease = tonumber(row.arguments[1]), tonumber(row.arguments[2]), row.arguments[3]:lower()
        assert(amount >= 0 and amount <= 1 and seconds >= 0 and delay >= 0)
        -- The source Ease enum has no IsSine value. ScenarioReader falls back to Linear.
        -- Numeric zero is Unset and DOTween uses its OutQuad default.
        assert(ease == "issine" or ease == "linear" or ease == "0")
        local request = {at = now + delay, to = amount, seconds = seconds, outQuad = ease == "0"}
        local index = #pending + 1
        while index > 1 and pending[index - 1].at > request.at do index = index - 1 end
        table.insert(pending, index, request)
    end

    return effect
end

function new_story_noise(host)
    local source = assert(host:find("opening-story-noise"))
    return new_story_ratio("DirectionMonochromeNoise", function(value)
        source:set("azure.screen-noise", "opacity", value)
    end)
end
