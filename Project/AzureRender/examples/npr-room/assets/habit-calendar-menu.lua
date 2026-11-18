function new_habit_calendar_menu(host, habits, label, weekdayLabel)
    local open, selectedId = false, nil
    local displayedDate, displayedToday, confirmArchive
    local rows = {}
    local menu = {}

    local function activeIndex()
        for index, row in ipairs(rows) do
            if row[1] == selectedId then return index end
        end
        return 1
    end

    local function redraw(date, today, preserveTitle)
        rows = habits:list()
        local index = activeIndex()
        local row = rows[index]
        selectedId = row and row[1]
        host:ui_visible("calendar-habit-current", row ~= nil)
        if not row then return end
        if not preserveTitle then host:ui_set_value("calendar-habit-title", row[2]) end
        host:ui_text("calendar-habit-position", string.format("%d / %d", index, #rows))
        host:ui_text("calendar-habit-complete", label("habitCompleted", "Completed") .. " · "
            .. (habits:isComplete(row, date) and label("habitOn", "On") or label("habitOff", "Off")))
        host:ui_text("calendar-habit-pause", habits:isPaused(row, today)
            and label("habitResume", "Resume") or label("habitPause", "Pause"))
        for day = 0, 6 do
            local name = weekdayLabel(day + 1)
            host:ui_text("calendar-habit-day-" .. day, row[4] & (1 << day) ~= 0 and name or "(" .. name .. ")")
        end
    end

    local function history(date)
        local names = habits:completed(date)
        host:ui_visible("calendar-habit-history", #names > 0 and not open)
        host:ui_text("calendar-habit-history-list", table.concat(names, "\n"))
    end

    local function finishTitle()
        if not open or not selectedId or not host:pressed("calendar-habit-save") then return nil end
        local renamed = habits:rename(selectedId, host:ui_value("calendar-habit-title"))
        host:ui_text("calendar-habit-status", renamed and "" or label("error", "Could not update the habit"))
        return renamed
    end

    local function showArchive(value)
        confirmArchive = value
        host:ui_visible("calendar-habit-confirm", value)
    end

    function menu:close()
        if finishTitle() == false then return false end
        open = false
        displayedDate, displayedToday = nil, nil
        host:ui_visible("calendar-habit-editor", false)
        showArchive(false)
        return true
    end

    function menu:update(date, today, languageChanged)
        local refresh = displayedDate ~= date or displayedToday ~= today or languageChanged
        if refresh then
            host:ui_text("calendar-habits-toggle", label("habitTitle", "Habit Tracker"))
            host:ui_text("calendar-habit-new", label("habitNew", "New Habit"))
            host:ui_text("calendar-habit-archive", label("habitArchive", "Delete"))
            host:ui_text("calendar-habit-archive-confirm", label("habitArchive", "Delete"))
            host:ui_text("calendar-habit-archive-cancel", label("cancel", "Cancel"))
        end
        local changed = finishTitle()
        if changed == false then return end
        if host:pressed("calendar-habits-toggle") then
            open = not open
            host:ui_visible("calendar-habit-editor", open)
            if open then redraw(date, today) end
            showArchive(false)
            refresh = true
        end
        if open then
            if confirmArchive then
                if host:pressed("calendar-habit-archive-confirm") then
                    changed = habits:archive(selectedId)
                    if changed then showArchive(false) end
                elseif host:pressed("calendar-habit-archive-cancel") then
                    showArchive(false)
                end
            elseif host:pressed("calendar-habit-new") then
                local id = habits:add("", today)
                if id then selectedId = id end
                changed = id ~= false
            elseif selectedId then
                local old = activeIndex()
                if host:pressed("calendar-habit-next") and rows[old + 1] then
                    selectedId, changed = rows[old + 1][1], true
                elseif host:pressed("calendar-habit-prev") and rows[old - 1] then
                    selectedId, changed = rows[old - 1][1], true
                elseif host:pressed("calendar-habit-complete") then
                    changed = habits:toggleCompletion(selectedId, date)
                elseif host:pressed("calendar-habit-pause") then
                    changed = habits:togglePause(selectedId, today)
                elseif host:pressed("calendar-habit-archive") then
                    showArchive(true)
                elseif host:pressed("calendar-habit-move-up") then
                    changed = habits:reorder(selectedId, -1)
                elseif host:pressed("calendar-habit-move-down") then
                    changed = habits:reorder(selectedId, 1)
                else
                    for day = 0, 6 do
                        if host:pressed("calendar-habit-day-" .. day) then
                            changed = habits:toggleWeekday(selectedId, day)
                            break
                        end
                    end
                end
            end
            if changed ~= nil then
                host:ui_text("calendar-habit-status", changed and "" or label("error", "Could not update the habit"))
            end
            if changed or refresh then redraw(date, today, not changed) end
        end
        if changed or refresh then history(date) end
        displayedDate, displayedToday = date, today
    end

    return menu
end
