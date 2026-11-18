local workSlot = "work-calendar"
local weekdaySlot = "calendar-first-day"
local weekdays = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"}
local monthLengths = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}

local function readFirstDay(runtime)
    local value = runtime:read_save(weekdaySlot)
    if type(value) ~= "table" then return 0, true end
    if type(value[1]) == "number" and value[1] > 1 then return 0, false end
    local day = value[2]
    if value[1] == 1 and type(day) == "number" and math.tointeger(day) and day >= 0 and day <= 6 then
        return day, true
    end
    return 0, true
end

local function monthLength(year, month)
    if month == 2 and year % 4 == 0 and (year % 100 ~= 0 or year % 400 == 0) then return 29 end
    return monthLengths[month]
end

local function firstWeekday(year, month)
    local previousYear = year - 1
    local days = 365 * previousYear + previousYear // 4 - previousYear // 100 + previousYear // 400 + 1
    for preceding = 1, month - 1 do days = days + monthLength(year, preceding) end
    return days % 7
end

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
            return result, true, value[3], active, value[5] == true
        end
    end
    return result, true, initial, 0, value[1] == 1 and value[3] == true
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
    local saved, _, progression, interrupted, completedTimer = readWorkDays(rewards)
    local pending = {}
    local pendingSettlements, pendingActive = {}, 0
    local pendingCompletion = false
    local recoverPending = interrupted > 0
    local lastDate, elapsed = nil, 0
    local calendar = {}
    local runtime = self
    local startDay, displayedMonth, displayedStart

    local function today()
        return runtime:local_datetime():sub(1, 8)
    end

    function calendar:flush()
        if next(pending) == nil and #pendingSettlements == 0 and not pendingCompletion then return true end
        local current, writable, earned, active, completed = readWorkDays(rewards)
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
        completed = completed or pendingCompletion
        if completed then document[rewards and 5 or 3] = true end
        if not runtime:try_write_save(workSlot, document) then return false end
        saved, pending, elapsed = current, {}, 0
        progression, pendingSettlements, pendingActive = earned, {}, 0
        completedTimer, pendingCompletion = completed, false
        return true
    end

    function calendar:settle(recordCompletion)
        pendingCompletion = pendingCompletion or (recordCompletion and not completedTimer) or false
        if rewards then
            pendingSettlements[#pendingSettlements + 1] = pendingActive
            pendingActive = 0
        end
        return self:flush()
    end

    function calendar:completed_timer()
        return completedTimer == true
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

    function calendar:update_month_view()
        if startDay == nil then startDay = readFirstDay(runtime) end
        for weekday = 0, 6 do
            if runtime:pressed("calendar-first-" .. weekday) then
                local _, writable = readFirstDay(runtime)
                if writable and runtime:try_write_save(weekdaySlot, {1, weekday}) then
                    startDay = weekday
                    runtime:ui_text("calendar-status", "")
                else
                    runtime:ui_text("calendar-status", "Could not save the start of week")
                end
                break
            end
        end
        local date = today()
        local monthKey = date:sub(1, 6)
        if displayedMonth == monthKey and displayedStart == startDay then return end
        displayedMonth, displayedStart = monthKey, startDay
        local year, month = tonumber(date:sub(1, 4)), tonumber(date:sub(5, 6))
        local offset = (firstWeekday(year, month) - startDay) % 7
        local days = monthLength(year, month)
        runtime:ui_text("calendar-month", string.format("%04d-%02d", year, month))
        runtime:ui_text("calendar-first-day", weekdays[startDay + 1])
        for column = 1, 7 do
            runtime:ui_text("calendar-weekday-" .. column, weekdays[(startDay + column - 1) % 7 + 1])
        end
        for cell = 1, 42 do
            local day = cell - offset
            runtime:ui_text("calendar-date-" .. cell, day >= 1 and day <= days and tostring(day) or "")
        end
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
