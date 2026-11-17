function new_story_body(select, states, routes)
    local motion, current, route = {}, nil, nil
    local preparation = {Text = true, ChangeMotion = true, AddTextLoop = true, DirectionGameStartCamera = true}

    local function play(number)
        if number == -1 or current == number then return end
        local bindings = route and route.signals or states
        select(assert(bindings[tostring(number)], "Missing opening body motion " .. tostring(number)))
        current = number
    end

    function motion:begin(chapter)
        current = nil
        route = routes and routes[chapter.group]
        if route then
            select(route.entry)
            current = 0
            return
        end
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
        current, route = nil, nil
    end

    return motion
end
