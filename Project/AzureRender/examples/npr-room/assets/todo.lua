local slot = "todo"
local lists, nextId, listIndex, taskIndex, ordered, open
local achievements, completed

local function dateValid(date)
    if date == "" then return true end
    local year, month, day = date:match("^(%d%d%d%d)%-(%d%d)%-(%d%d)$")
    if not year then return false end
    year, month, day = tonumber(year), tonumber(month), tonumber(day)
    local days = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
    if year % 4 == 0 and (year % 100 ~= 0 or year % 400 == 0) then days[2] = 29 end
    return year > 0 and days[month] ~= nil and day >= 1 and day <= days[month]
end

local function completedValid(value)
    if value == "" then return true end
    local y, m, d, h, minute, s = value:match("^(%d%d%d%d)(%d%d)(%d%d)(%d%d)(%d%d)(%d%d)$")
    return y ~= nil and dateValid(y .. "-" .. m .. "-" .. d)
        and tonumber(h) < 24 and tonumber(minute) < 60 and tonumber(s) < 60
end

local function load()
    local document = self:read_save(slot)
    local fallback = {{1, "", {}}}
    if type(document) ~= "table" or document[1] ~= 1 or type(document[2]) ~= "number"
        or document[2] % 1 ~= 0 or document[2] < 2 or type(document[3]) ~= "table" then
        return fallback, 2
    end
    local seen = {}
    local function register(id)
        if type(id) ~= "number" or id % 1 ~= 0 or id < 1 or id >= document[2] or seen[id] then return false end
        seen[id] = true
        return true
    end
    local value = document[3]
    for _, list in ipairs(value) do
        if type(list) ~= "table" or not register(list[1]) or type(list[2]) ~= "string" or type(list[3]) ~= "table" then
            return fallback, 2
        end
        for _, task in ipairs(list[3]) do
            if type(task) ~= "table" or not register(task[1]) or type(task[2]) ~= "string"
                or type(task[3]) ~= "string" or not completedValid(task[3])
                or type(task[4]) ~= "string" or not dateValid(task[4]) then
                return fallback, 2
            end
        end
    end
    if #value == 0 then return fallback, 2 end
    completed = document[4] == true
    return value, document[2]
end

local function status(text)
    self:ui_text("todo-status", text)
end

local function cue(id)
    local sound = self:find(id)
    if sound then sound:try_audio_play() end
end

local function publish(proposed, id, completedNow)
    local saved = self:read_save(slot)
    if type(saved) == "table" and type(saved[1]) == "number" and saved[1] > 1 then
        status("This task list requires a newer version.")
        return false
    end
    local document = {1, id, proposed}
    if completed or (achievements and completedNow) then document[4] = true end
    if not self:try_write_save(slot, document) then
        status("Could not save the task list.")
        return false
    end
    lists, nextId = proposed, id
    completed = document[4] == true
    if achievements and completed then achievements:once("COMPLETE_TODO_COUNT") end
    status("")
    return true
end

local function selectedRow()
    return ordered[taskIndex]
end

local function showTask(id)
    local list = lists[listIndex]
    local pending, completed = {}, {}
    for index, task in ipairs(list[3]) do
        local group = task[3] == "" and pending or completed
        group[#group + 1] = {index, task}
    end
    table.sort(completed, function(a, b)
        if a[2][3] == b[2][3] then return a[2][1] < b[2][1] end
        return a[2][3] > b[2][3]
    end)
    ordered = pending
    for _, row in ipairs(completed) do ordered[#ordered + 1] = row end
    taskIndex = 1
    for index, row in ipairs(ordered) do
        if row[2][1] == id then taskIndex = index; break end
    end
    self:ui_set_value("todo-list-title", list[2])
    self:ui_text("todo-list-position", string.format("%d / %d", listIndex, #lists))
    self:ui_text("todo-progress", string.format("%d / %d", #completed, #list[3]))
    self:ui_visible("todo-task-details", #ordered > 0)
    local row = selectedRow()
    if row then
        local task = row[2]
        self:ui_set_value("todo-task-text", task[2])
        self:ui_set_value("todo-task-due", task[4])
        self:ui_text("todo-task-position", string.format("%d / %d", taskIndex, #ordered))
        self:ui_text("todo-task-complete", task[3] == "" and "Complete" or "Reopen")
        self:ui_text("todo-task-state", task[3] == "" and "Working" or "Completed")
    end
    status("")
end

local function saveEdits()
    local current = lists[listIndex]
    local title = self:ui_value("todo-list-title")
    local tasks = current[3]
    local row = selectedRow()
    if row then
        local task = row[2]
        local text = self:ui_value("todo-task-text")
        local due = self:ui_value("todo-task-due")
        if not dateValid(due) then
            status("Enter a valid date as YYYY-MM-DD.")
            return false
        end
        if text ~= task[2] or due ~= task[4] then
            tasks = {table.unpack(tasks)}
            tasks[row[1]] = {task[1], text, task[3], due}
        end
    end
    if title == current[2] and tasks == current[3] then return true end
    local proposed = {table.unpack(lists)}
    proposed[listIndex] = {current[1], title, tasks}
    if not publish(proposed, nextId) then return false end
    if row then row[2] = tasks[row[1]] end
    return true
end

local function changeTask(taskAction)
    local row = selectedRow()
    if taskAction ~= "new" and not row then return end
    if not saveEdits() then return end
    local current = lists[listIndex]
    local tasks = {table.unpack(current[3])}
    local id, next = nextId, nextId
    local sound
    if taskAction == "new" then
        tasks[#tasks + 1] = {id, "", "", ""}
        next = nextId + 1
        sound = "settings-click"
    else
        local position, task = row[1], row[2]
        id = task[1]
        if taskAction == "delete" then
            table.remove(tasks, position)
            sound = "settings-cancel"
        elseif taskAction == "complete" then
            local now = task[3] == "" and self:local_datetime() or ""
            tasks[position] = {id, task[2], now, task[4]}
            sound = now == "" and "settings-cancel" or "todo-complete"
        else
            local direction = taskAction == "up" and -1 or 1
            local neighbor = ordered[taskIndex + direction]
            if task[3] ~= "" or not neighbor or neighbor[2][3] ~= "" then return end
            tasks[position], tasks[neighbor[1]] = tasks[neighbor[1]], tasks[position]
        end
    end
    local proposed = {table.unpack(lists)}
    proposed[listIndex] = {current[1], current[2], tasks}
    if publish(proposed, next, sound == "todo-complete") then
        if sound then cue(sound) end
        showTask(id)
    end
end

local function changeList(listAction)
    if listAction == "delete" and #lists == 1 then return end
    if not saveEdits() then return end
    local proposed = {table.unpack(lists)}
    if listAction == "new" then
        proposed[#proposed + 1] = {nextId, "", {}}
        if not publish(proposed, nextId + 1) then return end
        listIndex = #lists
        cue("settings-click")
    else
        table.remove(proposed, listIndex)
        if not publish(proposed, nextId) then return end
        listIndex = 1
        cue("settings-cancel")
    end
    showTask()
end

function init()
    achievements = new_room_achievements and new_room_achievements(self)
    completed = false
    lists, nextId = load()
    if achievements and completed then achievements:once("COMPLETE_TODO_COUNT") end
    listIndex, taskIndex, ordered, open = 1, 1, {}, false
    self:ui_visible("todo-toggle", true)
    self:ui_visible("todo-panel", false)
end

function update()
    if self:pressed("todo-toggle") then
        if open and not saveEdits() then return end
        open = not open
        self:ui_visible("todo-panel", open)
        if open then showTask() end
    elseif open then
        if self:pressed("todo-save") then saveEdits(); return end
        for _, name in ipairs({"new", "delete"}) do
            if self:pressed("todo-list-" .. name) then changeList(name); return end
        end
        for _, name in ipairs({"new", "complete", "delete", "up", "down"}) do
            if self:pressed("todo-task-" .. name) then changeTask(name); return end
        end
        local listDirection = self:pressed("todo-list-prev") and -1 or self:pressed("todo-list-next") and 1 or 0
        local destination = listIndex + listDirection
        if listDirection ~= 0 and lists[destination] and saveEdits() then
            listIndex = destination
            showTask()
        end
        local taskDirection = self:pressed("todo-task-prev") and -1 or self:pressed("todo-task-next") and 1 or 0
        local target = ordered[taskIndex + taskDirection]
        if taskDirection ~= 0 and target and saveEdits() then showTask(target[2][1]) end
    end
end
