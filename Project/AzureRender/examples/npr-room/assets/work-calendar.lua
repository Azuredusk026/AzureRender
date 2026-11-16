local workSlot = "work-calendar"

local function readWorkDays(rewards)
    local value = self:read_save(workSlot)
    local result = {}
    local initial = {1, 0, 0}
    if type(value) ~= "table" or type(value[1]) ~= "number" then return result, true, initial, 0 end
    if value[1] > (rewards and 2 or 1) then return result, false, initial, 0 end
    if (value[1] ~= 1 and value[1] ~= 2) or type(value[2]) ~= "table" then return result, true, initial, 0 end
    for _, year in ipairs(value[2]) do
        if type(year) ~= "table" or type(year[1]) ~= "string" or not year[1]:match("^%d%d%d%d$")
            or type(year[2]) ~= "table" then return {}, true, initial, 0 end
        for _, day in ipairs(year[2]) do
            if type(day) ~= "table" or type(day[1]) ~= "string" or not day[1]:match("^%d%d%d%d$")
                or type(day[2]) ~= "number" or day[2] <= 0 or day[2] == math.huge then return {}, true, initial, 0 end
            local date = year[1] .. day[1]
            if result[date] then return {}, true, initial, 0 end
            result[date] = day[2]
        end
    end
    if value[1] == 2 and type(value[3]) == "table" and #value[3] == 3 then
        local level, exp, points = table.unpack(value[3])
        local active = value[4]
        if type(level) == "number" and level >= 1 and math.tointeger(level)
            and type(exp) == "number" and exp >= 0 and exp < math.huge
            and type(points) == "number" and points >= 0 and math.tointeger(points)
            and type(active) == "number" and active >= 0 and active < math.huge then
            return result, true, value[3], active
        end
    end
    return result, true, initial, 0
end

local function encodeWorkDays(data)
    local years = {}
    for date, seconds in pairs(data) do
        local year = date:sub(1, 4)
        if not years[year] then years[year] = {} end
        years[year][#years[year] + 1] = {date:sub(5), seconds}
    end
    local entries = {}
    for year, days in pairs(years) do
        table.sort(days, function(a, b) return a[1] < b[1] end)
        entries[#entries + 1] = {year, days}
    end
    table.sort(entries, function(a, b) return a[1] < b[1] end)
    return entries
end

function new_work_calendar()
    local rewards = work_reward_config
    local saved, _, progression, interrupted = readWorkDays(rewards)
    local pending = {}
    local pendingSettlements, pendingActive = {}, 0
    local recoverPending = interrupted > 0
    local lastDate, elapsed = nil, 0
    local calendar = {}
    local runtime = self

    local function today()
        return runtime:local_datetime():sub(1, 8)
    end

    function calendar:flush()
        if next(pending) == nil and #pendingSettlements == 0 then return true end
        local current, writable, earned, active = readWorkDays(rewards)
        if not writable then return false end
        for day, delta in pairs(pending) do current[day] = (current[day] or 0) + delta end
        local days = encodeWorkDays(current)
        local document = {1, days}
        if rewards then
            for _, completed in ipairs(pendingSettlements) do
                earned = settle_work_reward(rewards, earned, active + completed, 0)
                active = 0
            end
            document = {2, days, earned, active + pendingActive}
        end
        if not runtime:try_write_save(workSlot, document) then return false end
        saved, pending, elapsed = current, {}, 0
        progression, pendingSettlements, pendingActive = earned, {}, 0
        return true
    end

    function calendar:settle()
        if rewards then
            pendingSettlements[#pendingSettlements + 1] = pendingActive
            pendingActive = 0
        end
        return self:flush()
    end

    function calendar:recover()
        if recoverPending then
            recoverPending = false
            self:settle()
        end
    end

    function calendar:progress()
        if rewards then return progression end
    end

    function calendar:advance(delta, working)
        local day = today()
        if lastDate and day ~= lastDate then self:flush() end
        lastDate = day
        if not working or delta <= 0 then return end
        pending[day] = (pending[day] or 0) + delta
        if rewards then pendingActive = pendingActive + delta end
        elapsed = elapsed + delta
        if elapsed >= 60 - 1e-7 then
            elapsed = 0
            self:flush()
        end
    end

    function calendar:timeFor(day)
        return (saved[day] or 0) + (pending[day] or 0)
    end

    function calendar:today()
        return math.min(86400, self:timeFor(today()))
    end

    function calendar:totalTime()
        local total = 0
        for day, seconds in pairs(saved) do total = total + math.min(86400, seconds + (pending[day] or 0)) end
        for day, seconds in pairs(pending) do
            if not saved[day] then total = total + math.min(86400, seconds) end
        end
        return total
    end

    return calendar
end
