local workSlot = "work-calendar"

local function readWorkDays()
    local value = self:read_save(workSlot)
    local result = {}
    if type(value) ~= "table" or type(value[1]) ~= "number" then return result, true end
    if value[1] > 1 then return result, false end
    if value[1] ~= 1 or type(value[2]) ~= "table" then return result, true end
    for _, year in ipairs(value[2]) do
        if type(year) ~= "table" or type(year[1]) ~= "string" or not year[1]:match("^%d%d%d%d$")
            or type(year[2]) ~= "table" then return {}, true end
        for _, day in ipairs(year[2]) do
            if type(day) ~= "table" or type(day[1]) ~= "string" or not day[1]:match("^%d%d%d%d$")
                or type(day[2]) ~= "number" or day[2] <= 0 or day[2] == math.huge then return {}, true end
            local date = year[1] .. day[1]
            if result[date] then return {}, true end
            result[date] = day[2]
        end
    end
    return result, true
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
    return {1, entries}
end

function new_work_calendar()
    local saved = readWorkDays()
    local pending = {}
    local lastDate, elapsed = nil, 0
    local calendar = {}
    local runtime = self

    local function today()
        return runtime:local_datetime():sub(1, 8)
    end

    function calendar:flush()
        if next(pending) == nil then return true end
        local current, writable = readWorkDays()
        if not writable then return false end
        for day, delta in pairs(pending) do current[day] = (current[day] or 0) + delta end
        if not runtime:try_write_save(workSlot, encodeWorkDays(current)) then return false end
        saved, pending, elapsed = current, {}, 0
        return true
    end

    function calendar:advance(delta, working)
        local day = today()
        if lastDate and day ~= lastDate then self:flush() end
        lastDate = day
        if not working or delta <= 0 then return end
        pending[day] = (pending[day] or 0) + delta
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
