function timer_voice_scenario(rules, event, countup, context, random)
    random = random or math.random
    local function occurs(key) return random() * 99.9 < rules[key].percent end
    if event == "work" then
        if not countup and context.pomodoroRunning and context.round == 1
            and context.planned >= rules.longWorkStart.seconds and occurs("longWorkStart") then
            return 1001
        end
        local previous = context.previousWorkEnd
        if previous and previous > 0 and (not context.pomodoroRunning or context.round == 1)
            and context.now - previous <= rules.continuousWorkStart.seconds and occurs("continuousWorkStart") then
            return countup and 3001 or 1000
        end
        return countup and 3000 or 4
    end
    if event == "break" then
        if context.intervalWorked <= rules.shortWorkedBreakStart.seconds and occurs("shortWorkedBreakStart") then
            return countup and 3101 or 1100
        end
        if context.intervalWorked >= rules.longWorkedBreakStart.seconds and occurs("longWorkedBreakStart") then
            return countup and 3102 or 1101
        end
        return countup and 3100 or 5
    end
    assert(event == "finish")
    if context.totalWorked >= rules.longWorkFinish.seconds and occurs("longWorkFinish") then
        return countup and 3202 or 1200
    end
    if context.totalWorked <= rules.shortWorkFinish.seconds and occurs("shortWorkFinish") then
        return countup and 3201 or 1201
    end
    if not countup and context.midway and occurs("midwayFinish") then return 1202 end
    return countup and 3200 or 6
end

function new_timer_voice_bank(groups, random)
    random = random or math.random
    local stacks, previousLast, peers = {}, {}, {}
    for _, pair in ipairs({{4, 3000}, {1000, 3001}, {5, 3100}, {1100, 3101}, {1101, 3102},
                           {6, 3200}, {1200, 3202}, {1201, 3201}}) do
        peers[pair[1]], peers[pair[2]] = pair[2], pair[1]
    end

    local function stack(scenario)
        local pending = stacks[scenario] or {}
        if #pending > 0 then return pending end
        for index = 1, #groups[scenario] do pending[index] = index end
        for index = #pending, 2, -1 do
            local other = random(index)
            pending[index], pending[other] = pending[other], pending[index]
        end
        if #pending > 1 and pending[1] == previousLast[scenario] then
            pending[#pending + 1] = table.remove(pending, 1)
        end
        previousLast[scenario] = pending[#pending]
        stacks[scenario] = pending
        return pending
    end

    local bank = {}
    function bank:take(scenario)
        local row = groups[scenario][table.remove(stack(scenario), 1)]
        local paired = peers[scenario]
        if paired and groups[paired] then
            local alternatives = stack(paired)
            for offset, index in ipairs(alternatives) do
                if groups[paired][index].localization == row.localization then
                    table.remove(alternatives, offset)
                    break
                end
            end
        end
        return row
    end
    return bank
end
