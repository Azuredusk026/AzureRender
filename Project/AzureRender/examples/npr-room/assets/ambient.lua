local slot = "ambient"
local open = false
local selected, page, pageSize = 1, 1, 7
local state, indexes = {}, {}

local function apply(index)
    local row = state[index]
    local effectiveVolume = row.muted and 0 or row.volume / 100
    if effectiveVolume == 0 and not row.started then return end
    local source = assert(self:find(sounds[index][2]))
    source:set("azure.audio-source", "volume", effectiveVolume)
    if not row.started then
        source:set("azure.audio-source", "asset", sounds[index][3])
        source:audio_play()
        row.started = true
    end
end

local function save()
    local rows = {}
    for index, source in ipairs(sounds) do
        local row = state[index]
        if row.volume ~= 0 or not row.muted then
            rows[#rows + 1] = {source[1], row.volume, row.muted}
        end
    end
    self:ui_text("ambient-status",
        self:try_write_save(slot, {1, rows}) and "" or "Ambient sound settings could not be saved")
end

local function displaySelection()
    local row = state[selected]
    self:ui_text("ambient-title", sounds[selected][4])
    self:ui_text("ambient-volume", (row.muted and "0" or tostring(row.volume)) .. "%")
    self:ui_text("ambient-mute", row.muted and "Enable" or "Mute")
end

local function displayPage(direction)
    local pages = math.ceil(#sounds / pageSize)
    page = math.max(1, math.min(pages, page + direction))
    local first = (page - 1) * pageSize + 1
    for i, sound in ipairs(sounds) do
        self:ui_visible("ambient-select-" .. sound[1], i >= first and i < first + pageSize)
    end
    self:ui_text("ambient-page", string.format("%d / %d", page, pages))
end

function init()
    for index, sound in ipairs(sounds) do
        indexes[sound[1]] = index
        state[index] = {volume = 0, muted = true, started = false}
    end
    local saved = self:read_save(slot)
    if type(saved) == "table" and saved[1] == 1 and type(saved[2]) == "table" then
        for _, row in ipairs(saved[2]) do
            if type(row) == "table" and type(row[2]) == "number" and row[2] >= 0 and row[2] <= 100
                and row[2] % 1 == 0 and type(row[3]) == "boolean" and indexes[row[1]] then
                local value = state[indexes[row[1]]]
                value.volume, value.muted = row[2], row[3] or row[2] == 0
            end
        end
    end
    for index = 1, #sounds do apply(index) end
    self:ui_visible("ambient-panel", false)
    displaySelection()
end

local function changeVolume(row, difference)
    local volume = math.max(0, math.min(100, (row.muted and 0 or row.volume) + difference))
    if volume == 0 then
        row.muted = true
    else
        row.volume, row.muted = volume, false
    end
end

function update()
    if self:pressed("ambient-toggle") then
        open = not open
        self:ui_visible("ambient-panel", open)
        if open then displayPage(0) end
    end
    if not open then return end
    local direction = self:pressed("ambient-page-prev") and -1 or self:pressed("ambient-page-next") and 1 or 0
    if direction ~= 0 then displayPage(direction); return end
    for index, sound in ipairs(sounds) do
        if self:pressed("ambient-select-" .. sound[1]) then selected = index; displaySelection(); return end
    end
    local row = state[selected]
    if self:pressed("ambient-mute") then
        if row.muted and row.volume == 0 then row.volume = 50 end
        row.muted = not row.muted
    elseif self:pressed("ambient-quiet") then
        changeVolume(row, -5)
    elseif self:pressed("ambient-loud") then
        changeVolume(row, 5)
    else
        return
    end
    apply(selected)
    save()
    displaySelection()
end
