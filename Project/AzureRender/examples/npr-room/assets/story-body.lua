function new_story_body(select, states)
    local motion, current = {}, nil
    local preparation = {Text = true, ChangeMotion = true, AddTextLoop = true, DirectionGameStartCamera = true}

    local function play(number)
        if number == -1 or current == number then return end
        select(assert(states[tostring(number)], "Missing opening body motion " .. tostring(number)))
        current = number
    end

    function motion:begin(chapter)
        current = nil
        for _, row in ipairs(chapter.commands) do
            if preparation[row.command] then
                local number = row.bodyMotion or -1
                play(number == -1 and 0 or number)
                return
            end
        end
    end

    function motion:row(row)
        play(row.bodyMotion or -1)
    end

    function motion:finish()
        select("idle")
        current = nil
    end

    return motion
end
