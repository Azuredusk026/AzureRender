local slot = "music"
local current = 1
local order = {}
local playing, started = false, false
local open, listOpen, repeatOne = false, false, false
local volume, muted, shuffle = 50, false, false
local page, pageSize = 1, 6

local function gain()
    self:set("azure.audio-source", "volume", muted and 0 or volume / 100)
end

local function applyPreferences()
    gain()
    if not self:try_write_save(slot, {1, playing, volume, muted, shuffle, repeatOne}) then
        self:ui_text("music-status", "Music settings could not be saved")
    else
        self:ui_text("music-status", "")
    end
end

local function setOrder()
    for i = 1, #tracks do order[i] = i end
    if not shuffle then return end
    order[1], order[current] = order[current], order[1]
    for i = #order, 3, -1 do
        local j = math.random(2, i)
        order[i], order[j] = order[j], order[i]
    end
end

local function updateDisplay()
    local track = tracks[current]
    self:ui_text("music-title", track[3])
    self:ui_text("music-credit", track[4])
    self:ui_text("music-play", playing and "Pause" or "Play")
    self:ui_text("music-volume", tostring(volume) .. "%")
    self:ui_text("music-mute", muted and "Unmute" or "Mute")
    self:ui_text("music-repeat", repeatOne and "Repeat one" or "Repeat off")
    self:ui_text("music-shuffle", shuffle and "Shuffle on" or "Shuffle off")
end

local function start(index)
    if started then self:audio_stop() end
    current = index
    self:set("azure.audio-source", "asset", tracks[current][2])
    self:audio_play()
    playing, started = true, true
end

local function move(direction)
    for i, index in ipairs(order) do
        if index == current then
            start(order[(i - 1 + direction) % #order + 1])
            return
        end
    end
end

local function showPage(direction)
    local pages = math.ceil(#tracks / pageSize)
    page = math.max(1, math.min(pages, page + direction))
    local lower = (page - 1) * pageSize + 1
    local upper = math.min(#tracks, lower + pageSize - 1)
    for i = 1, #tracks do self:ui_visible("music-select-" .. i, i >= lower and i <= upper) end
    self:ui_text("music-page", string.format("%d / %d", page, pages))
end

function init()
    local saved = self:read_save(slot)
    local autoplay = true
    if type(saved) == "table" and saved[1] == 1 and type(saved[2]) == "boolean"
        and type(saved[3]) == "number" and saved[3] >= 0 and saved[3] <= 100 and saved[3] % 1 == 0
        and type(saved[4]) == "boolean" and type(saved[5]) == "boolean" then
        autoplay, volume, muted, shuffle = saved[2], saved[3], saved[4], saved[5]
        repeatOne = saved[6] == true
    end
    setOrder()
    gain()
    if autoplay then start(current) end
    self:ui_visible("music-panel", false)
    self:ui_visible("music-list-box", false)
    updateDisplay()
end

local function processControls()
    if self:pressed("music-toggle") then
        open = not open
        self:ui_visible("music-panel", open)
    end
    if not open then return false end
    if self:pressed("music-list") then
        listOpen = not listOpen
        self:ui_visible("music-list-box", listOpen)
        if listOpen then showPage(0) end
        return false
    elseif self:pressed("music-play") then
        if started then
            self:audio_pause(playing)
            playing = not playing
        else
            start(current)
        end
    elseif self:pressed("music-previous") then
        if not started or self:audio_position() >= 5 then start(current) else move(-1) end
    elseif self:pressed("music-next") then
        if started then move(1) else start(current) end
    elseif self:pressed("music-repeat") then
        repeatOne = not repeatOne
    elseif self:pressed("music-shuffle") then
        shuffle = not shuffle
        setOrder()
    elseif self:pressed("music-quiet") then
        volume = math.max(0, volume - 5)
    elseif self:pressed("music-loud") then
        volume = math.min(100, volume + 5)
    elseif self:pressed("music-mute") then
        muted = not muted
    elseif listOpen then
        local pageDirection = self:pressed("music-page-prev") and -1 or self:pressed("music-page-next") and 1 or 0
        if pageDirection ~= 0 then
            showPage(pageDirection)
            return false
        end
        for i = 1, #tracks do
            if self:pressed("music-select-" .. i) then
                shuffle = false
                setOrder()
                start(i)
                return true
            end
        end
        return false
    else
        return false
    end
    return true
end

function update()
    if processControls() then
        applyPreferences()
        updateDisplay()
    end
    if playing and not self:audio_playing() and self:audio_position() >= tracks[current][5] then
        if repeatOne then start(current) else move(1) end
        updateDisplay()
    end
end
