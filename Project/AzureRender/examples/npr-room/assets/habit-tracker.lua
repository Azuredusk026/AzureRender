function new_room_habits(host, validDate, weekday)
    local slot = "habit-tracker"
    local habits = {}

    local function read()
        local document = host:read_save(slot)
        local empty = {1, 1, {}}
        if type(document) ~= "table" then return empty, true end
        if type(document[1]) == "number" and document[1] > 1 then return empty, false end
        if document[1] ~= 1 or type(document[2]) ~= "number" or not math.tointeger(document[2])
            or document[2] < 1 or type(document[3]) ~= "table" then return empty, true end
        local seen = {}
        for _, row in ipairs(document[3]) do
            if type(row) ~= "table" or type(row[1]) ~= "number" or not math.tointeger(row[1])
                or row[1] < 1 or row[1] >= document[2] or seen[row[1]] or type(row[2]) ~= "string"
                or not validDate(row[3]) or type(row[4]) ~= "number" or not math.tointeger(row[4])
                or row[4] < 0 or row[4] > 255 or type(row[5]) ~= "boolean"
                or type(row[6]) ~= "table" or type(row[7]) ~= "table" then return empty, true end
            seen[row[1]] = true
            for _, pause in ipairs(row[6]) do
                if type(pause) ~= "table" or not validDate(pause[1])
                    or (pause[2] ~= "" and (not validDate(pause[2]) or pause[2] <= pause[1])) then
                    return empty, true
                end
            end
            for _, day in ipairs(row[7]) do
                if not validDate(day) then return empty, true end
            end
        end
        return document, true
    end

    local function rowIndex(rows, id)
        for index, row in ipairs(rows) do
            if row[1] == id and not row[5] then return index end
        end
    end

    local function paused(row, date)
        for _, range in ipairs(row[6]) do
            if range[1] <= date and (range[2] == "" or date < range[2]) then return true end
        end
        return false
    end

    local function completionIndex(row, date)
        for index, value in ipairs(row[7]) do
            if date == value then return index end
        end
    end

    local function modify(id, edit)
        local document, writable = read()
        if not writable then return false end
        local rows = document[3]
        local index = rowIndex(rows, id)
        if not index or not edit(rows[index], rows, index) then return false end
        return host:try_write_save(slot, document)
    end

    function habits:list()
        local document = read()
        local result = {}
        for _, row in ipairs(document[3]) do
            if not row[5] then result[#result + 1] = row end
        end
        return result
    end

    function habits:completed(date)
        local document = read()
        local active, archived = {}, {}
        for _, row in ipairs(document[3]) do
            if completionIndex(row, date) then
                local group = row[5] and archived or active
                group[#group + 1] = row[2]
            end
        end
        for _, name in ipairs(archived) do active[#active + 1] = name end
        return active
    end

    function habits:add(title, date)
        if type(title) ~= "string" or not validDate(date) then return false end
        local document, writable = read()
        if not writable then return false end
        local id = document[2]
        document[2] = id + 1
        document[3][#document[3] + 1] = {id, title, date, 255, false, {}, {}}
        return host:try_write_save(slot, document) and id
    end

    function habits:rename(id, title)
        if type(title) ~= "string" then return false end
        return modify(id, function(row) row[2] = title; return true end)
    end

    function habits:archive(id)
        return modify(id, function(row, rows, index)
            row[5], row[6] = true, {}
            table.remove(rows, index)
            rows[#rows + 1] = row
            return true
        end)
    end

    function habits:reorder(id, direction)
        if direction ~= -1 and direction ~= 1 then return false end
        return modify(id, function(row, rows, index)
            local nextIndex = index + direction
            while rows[nextIndex] and rows[nextIndex][5] do nextIndex = nextIndex + direction end
            if not rows[nextIndex] then return false end
            rows[index], rows[nextIndex] = rows[nextIndex], row
            return true
        end)
    end

    function habits:isPaused(row, date)
        return paused(row, date)
    end

    function habits:isComplete(row, date)
        return completionIndex(row, date) ~= nil
    end

    function habits:toggleWeekday(id, day)
        if type(day) ~= "number" or not math.tointeger(day) or day < 0 or day > 6 then return false end
        return modify(id, function(row) row[4] = row[4] ~ (1 << day); return true end)
    end

    function habits:togglePause(id, date)
        if not validDate(date) then return false end
        return modify(id, function(row)
            if date < row[3] then return false end
            local pauses = row[6]
            while pauses[#pauses] and pauses[#pauses][1] > date do table.remove(pauses) end
            local range = pauses[#pauses]
            if range and (range[2] == "" or date < range[2]) then
                if date == range[1] then table.remove(pauses) else range[2] = date end
            elseif range and range[2] == date then
                range[2] = ""
            else
                pauses[#pauses + 1] = {date, ""}
            end
            return true
        end)
    end

    function habits:toggleCompletion(id, date)
        if not validDate(date) then return false end
        return modify(id, function(row)
            local index = completionIndex(row, date)
            if index then
                table.remove(row[7], index)
            elseif date >= row[3] and not paused(row, date) and row[4] & (1 << weekday(date)) ~= 0 then
                row[7][#row[7] + 1] = date
                table.sort(row[7])
            else
                return false
            end
            return true
        end)
    end

    return habits
end
