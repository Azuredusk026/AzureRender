local workSlot = "work-calendar"
local weekdaySlot = "calendar-first-day"
local weekdays = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"}
local monthLengths = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
local workTimeFields = {"calendar-edit-hours", "calendar-edit-minutes", "calendar-edit-seconds"}
local todoActions = {
    "todo-toggle", "todo-save",
    "todo-list-new", "todo-list-delete", "todo-list-prev", "todo-list-next",
    "todo-task-new", "todo-task-complete", "todo-task-delete", "todo-task-up", "todo-task-down",
    "todo-task-prev", "todo-task-next",
}

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

local function validDate(date)
    if type(date) ~= "string" or not date:match("^%d%d%d%d%d%d%d%d$") then return false end
    local year, month, day = tonumber(date:sub(1, 4)), tonumber(date:sub(5, 6)), tonumber(date:sub(7, 8))
    return year >= 1 and month >= 1 and month <= 12 and day >= 1 and day <= monthLength(year, month)
end

local function dateWeekday(date)
    local year, month, day = tonumber(date:sub(1, 4)), tonumber(date:sub(5, 6)), tonumber(date:sub(7, 8))
    return (firstWeekday(year, month) + day - 1) % 7
end

local function workTimeParts(seconds)
    seconds = math.floor(math.min(86400, seconds) + 1e-7)
    return {seconds // 3600, seconds // 60 % 60, seconds % 60}
end

local function completedTodoText(runtime, date)
    local document = runtime:read_save("todo")
    if type(document) ~= "table" or document[1] ~= 1 or type(document[3]) ~= "table" then return "" end
    local matches = {}
    for _, list in ipairs(document[3]) do
        if type(list) == "table" and type(list[3]) == "table" then
            for _, task in ipairs(list[3]) do
                if type(task) == "table" and type(task[1]) == "number" and type(task[2]) == "string"
                    and type(task[3]) == "string" then
                    local hour, minute, second = task[3]:match("^" .. date .. "(%d%d)(%d%d)(%d%d)$")
                    if hour and tonumber(hour) < 24 and tonumber(minute) < 60 and tonumber(second) < 60 then
                        matches[#matches + 1] = {task[1], hour .. ":" .. minute .. ":" .. second, task[2]}
                    end
                end
            end
        end
    end
    table.sort(matches, function(a, b)
        if a[2] == b[2] then return a[1] < b[1] end
        return a[2] < b[2]
    end)
    local rows = {}
    for index, task in ipairs(matches) do
        rows[index] = task[2] .. (task[3] == "" and "" or " " .. task[3])
    end
    return table.concat(rows, "\n")
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

function new_work_calendar(translate)
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
    local browsedMonth, selectedDate, editingDate
    local displayedWorkTime
    local displayedTodoDate, refreshTodo
    local diaryDate, diaryText, diaryDraft
    local habitMenu

    local function today()
        return runtime:local_datetime():sub(1, 8)
    end

    local function publish(editedDate, seconds)
        if not editedDate and next(pending) == nil and #pendingSettlements == 0 and not pendingCompletion then
            return true
        end
        local current, writable, earned, active, completed = readWorkDays(rewards)
        if not writable then return false end
        for day, delta in pairs(pending) do current[day] = (current[day] or 0) + delta end
        if editedDate then current[editedDate] = seconds > 0 and seconds or nil end
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

    function calendar:flush()
        return publish()
    end

    function calendar:setTimeFor(day, seconds)
        if not validDate(day) or type(seconds) ~= "number" or not math.tointeger(seconds)
            or seconds < 0 or seconds > 86400 then return false end
        return publish(day, seconds)
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

    local function weekdayLabel(index)
        return translate and calendar_labels and translate(calendar_labels.weekdays[index]) or weekdays[index]
    end

    local function calendarLabel(key, fallback)
        local labels = calendar_labels
        return translate and labels and labels[key] and translate(labels[key]) or fallback
    end

    local function renderEditFields()
        displayedWorkTime = workTimeParts(calendar:timeFor(editingDate))
        for index, field in ipairs(workTimeFields) do
            runtime:ui_set_value(field, string.format("%02d", displayedWorkTime[index]))
        end
    end

    local function submitEdit()
        local values = workTimeParts(calendar:timeFor(editingDate))
        local changed = false
        for index, field in ipairs(workTimeFields) do
            local input = runtime:ui_value(field)
            if input ~= "" then
                local value = math.tointeger(tonumber(input))
                if not value then return false end
                values[index] = math.max(0, math.min(index == 1 and 24 or 59, value))
                changed = changed or values[index] ~= displayedWorkTime[index]
            end
        end
        if not changed then return true end
        local hours, minutes, seconds = table.unpack(values)
        return calendar:setTimeFor(editingDate, math.min(86400, hours * 3600 + minutes * 60 + seconds))
    end

    local function commitEditor()
        if editingDate and runtime:pressed("calendar-submit-time") then
            local stored = submitEdit()
            runtime:ui_text("calendar-status", stored and "" or calendarLabel("error", "Could not save work time"))
            if stored then renderEditFields() end
        end
    end

    local function showEditor(day)
        editingDate = day
        runtime:ui_visible("calendar-work-time-editor", day ~= nil)
        runtime:ui_visible("calendar-selected-time", day == nil)
        if day then renderEditFields() end
    end

    local function readDiary(day)
        local document = runtime:read_save("calendar-diary-" .. day)
        if type(document) ~= "table" then return "", true end
        if type(document[1]) == "number" and document[1] > 1 then return "", false end
        local value = document[1] == 1 and type(document[2]) == "string" and document[2] or ""
        return value, true
    end

    local function showDiary(day)
        diaryDate = day
        runtime:ui_visible("calendar-diary-editor", day ~= nil)
        runtime:ui_text("calendar-diary-status", "")
        if day then
            diaryText = readDiary(day)
            diaryDraft = diaryText
            runtime:ui_set_value("calendar-diary-text", diaryText)
        end
    end

    local function storeDiary(value)
        if value == diaryText then return true end
        -- The original input field limits the UTF-16 length to 5,000.
        local _, codePoints = value:gsub("[^\128-\191]", "")
        local _, supplementary = value:gsub("[\240-\244]", "")
        local _, writable = readDiary(diaryDate)
        if not writable or codePoints + supplementary > 5000
            or not runtime:try_write_save("calendar-diary-" .. diaryDate, {1, value}) then return false end
        diaryText = value
        return true
    end

    local function saveDiary()
        if not diaryDate then return true end
        local stored = storeDiary(runtime:ui_value("calendar-diary-text"))
        runtime:ui_text("calendar-diary-status", stored and "" or calendarLabel("error", "Could not save the diary"))
        return stored
    end

    function calendar:save_open_diary()
        if diaryDate then return storeDiary(diaryDraft) end
        return true
    end

    function calendar:close_month_view()
        if not saveDiary() then return false end
        if habitMenu and not habitMenu:close() then return false end
        if diaryDate then showDiary(nil) end
        displayedTodoDate = nil
        if editingDate then
            commitEditor()
            showEditor(nil)
        end
        return true
    end

    function calendar:update_month_view(languageChanged)
        commitEditor()
        if diaryDate and runtime:pressed("calendar-diary-cancel") then
            showDiary(nil)
        elseif diaryDate and runtime:pressed("calendar-diary-save") then
            saveDiary()
        end
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
        local monthIndex = browsedMonth or tonumber(date:sub(1, 4)) * 12 + tonumber(date:sub(5, 6)) - 1
        local year, month = monthIndex // 12, monthIndex % 12 + 1
        if runtime:pressed("calendar-prev-month") and (month > 1 or year > 2025) then
            monthIndex = monthIndex - 1
            browsedMonth = monthIndex
        elseif runtime:pressed("calendar-next-month") and (month < 12 or year < 2998) then
            monthIndex = monthIndex + 1
            browsedMonth = monthIndex
        end
        year, month = monthIndex // 12, monthIndex % 12 + 1
        local offset = (firstWeekday(year, month) - startDay) % 7
        local days = monthLength(year, month)
        if displayedMonth ~= monthIndex or displayedStart ~= startDay or languageChanged then
            displayedMonth, displayedStart = monthIndex, startDay
            runtime:ui_text("calendar-month", string.format("%04d-%02d", year, month))
            runtime:ui_text("calendar-first-day-label", calendarLabel("firstDay", "Start day of the week"))
            runtime:ui_text("calendar-work-label", calendarLabel("workTime", "Work Time"))
            runtime:ui_text("calendar-edit-time", calendarLabel("editTime", "Edit Work Time"))
            runtime:ui_text("calendar-diary-toggle", calendarLabel("diary", "Enter Diary"))
            runtime:ui_text("calendar-diary-cancel", calendarLabel("cancel", "Cancel"))
            runtime:ui_text("calendar-first-day", weekdayLabel(startDay + 1))
            for column = 1, 7 do
                runtime:ui_text("calendar-first-" .. (column - 1), weekdayLabel(column))
                runtime:ui_text("calendar-weekday-" .. column, weekdayLabel((startDay + column - 1) % 7 + 1))
            end
            for cell = 1, 42 do
                local day = cell - offset
                local valid = day >= 1 and day <= days
                runtime:ui_text("calendar-date-" .. cell, valid and tostring(day) or "")
                runtime:ui_visible("calendar-date-" .. cell, valid)
            end
        end
        for day = 1, days do
            if runtime:pressed("calendar-day-" .. (offset + day)) then
                local requested = string.format("%04d%02d%02d", year, month, day)
                if saveDiary() then
                    selectedDate = requested
                    if diaryDate and diaryDate ~= requested then showDiary(requested) end
                end
                break
            end
        end
        local selected = selectedDate or date
        if editingDate and editingDate ~= selected then
            showEditor(nil)
        end
        if runtime:pressed("calendar-edit-time") then
            showEditor(editingDate == nil and selected or nil)
        end
        if runtime:pressed("calendar-diary-toggle") then
            if not diaryDate then
                selectedDate = selected
                showDiary(selected)
            elseif saveDiary() then
                showDiary(nil)
            end
        end
        if diaryDate then diaryDraft = runtime:ui_value("calendar-diary-text") end
        if new_room_habits and new_habit_calendar_menu then
            habitMenu = habitMenu or new_habit_calendar_menu(runtime,
                new_room_habits(runtime, validDate, dateWeekday), calendarLabel, weekdayLabel)
            habitMenu:update(selected, date, languageChanged)
        end
        if displayedTodoDate ~= selected or refreshTodo or languageChanged then
            local tasks = completedTodoText(runtime, selected)
            runtime:ui_text("calendar-completed-tasks-label", calendarLabel("completedTasks", "Completed Tasks"))
            runtime:ui_text("calendar-completed-tasks", tasks)
            runtime:ui_visible("calendar-completions", tasks ~= "")
            displayedTodoDate = selected
        end
        refreshTodo = false
        for _, action in ipairs(todoActions) do
            if runtime:pressed(action) then
                refreshTodo = true
                break
            end
        end
        local label = selected:sub(1, 4) .. "-" .. selected:sub(5, 6) .. "-" .. selected:sub(7, 8)
        return label, math.min(86400, self:timeFor(selected))
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
