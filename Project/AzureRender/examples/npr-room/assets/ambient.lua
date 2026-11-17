local slot = "ambient"
local open = false
local selected, page, pageSize = 1, 1, 7
local state, indexes = {}, {}
local access, wallet, confirmPurchase, cityWindow

local function accessFor(index)
    if access then return access:state(sounds[index][1]) end
    return "open"
end

local function apply(index)
    if accessFor(index) ~= "open" then return end
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

local function displaySelection(showAccess)
    local row = state[selected]
    local kind, hint = accessFor(selected)
    self:ui_text("ambient-title", kind == "open" and sounds[selected][4] or "Locked")
    self:ui_text("ambient-volume", (row.muted and "0" or tostring(row.volume)) .. "%")
    self:ui_text("ambient-mute", row.muted and "Enable" or "Mute")
    if access then
        for _, name in ipairs({"quiet", "volume", "loud", "mute"}) do
            self:ui_visible("ambient-" .. name, kind == "open")
        end
        if showAccess then self:ui_text("ambient-status", hint or "") end
    end
    if wallet then
        local name = environment_access[sounds[selected][1]][4]
        local price, affordable
        if confirmPurchase and kind == "locked" then price, affordable = wallet:offer(name) end
        self:ui_visible("ambient-purchase", price ~= nil)
        self:ui_visible("ambient-purchase-confirm", price ~= nil and affordable == true)
        self:ui_text("ambient-purchase-balance", string.format("Points: %d", wallet:balance()))
        if price then
            self:ui_text("ambient-purchase-price", name .. " · " .. price .. " points")
            if showAccess then self:ui_text("ambient-status", affordable and "" or "Not enough points") end
        end
    end
    if cityWindow then
        local show = sounds[selected][1] == city_sound_id and kind == "open"
        self:ui_visible("ambient-city-controls", show)
        if show then
            self:ui_text("ambient-city-window", cityWindow:active() and "Hide window view" or "Show window view")
            self:ui_text("ambient-city-combined",
                (cityWindow:active() or not row.muted) and "Turn off environment" or "Turn on environment")
        end
    end
end

local function displayPage(direction)
    local listed = {}
    for index, sound in ipairs(sounds) do
        local kind = accessFor(index)
        self:ui_visible("ambient-select-" .. sound[1], false)
        self:ui_text("ambient-select-" .. sound[1], kind == "open" and sound[4] or "Locked")
        if kind ~= "expired" then listed[#listed + 1] = sound end
    end
    local pages = math.ceil(#listed / pageSize)
    page = math.max(1, math.min(pages, page + direction))
    local first = (page - 1) * pageSize + 1
    for i = first, math.min(#listed, first + pageSize - 1) do
        self:ui_visible("ambient-select-" .. listed[i][1], true)
    end
    self:ui_text("ambient-page", string.format("%d / %d", page, pages))
end

function init()
    if new_room_wallet and wallet_catalog then wallet = new_room_wallet(self, wallet_catalog) end
    if new_ambient_access then access = new_ambient_access(wallet) end
    if new_room_window and city_window_node then
        cityWindow = new_room_window(self, "City", city_window_node,
            function() return access:state(city_sound_id) == "open" end,
            function() return wallet:bought("City") end)
    end
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
    displaySelection(true)
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
        confirmPurchase = false
        self:ui_visible("ambient-panel", open)
        if open then
            if access then access:refresh() end
            displayPage(0)
            displaySelection(true)
        end
    end
    if not open then return end
    local direction = self:pressed("ambient-page-prev") and -1 or self:pressed("ambient-page-next") and 1 or 0
    if direction ~= 0 then
        confirmPurchase = false
        displayPage(direction)
        displaySelection(true)
        return
    end
    for index, sound in ipairs(sounds) do
        if accessFor(index) ~= "expired" and self:pressed("ambient-select-" .. sound[1]) then
            selected = index
            confirmPurchase = wallet ~= nil and environment_access[sound[1]][1] == "purchase"
            displaySelection(true)
            return
        end
    end
    if confirmPurchase then
        if self:pressed("ambient-purchase-cancel") then
            confirmPurchase = false
            displaySelection(true)
            return
        end
        if self:pressed("ambient-purchase-confirm") then
            local name = environment_access[sounds[selected][1]][4]
            local purchased, reason = wallet:purchase(name)
            if purchased then
                confirmPurchase = false
                self:find("room-buy-item"):audio_play()
                if cityWindow and sounds[selected][1] == city_sound_id then cityWindow:restore() end
                local row = state[selected]
                if row.volume == 0 then row.volume = 50 end
                row.muted = false
                apply(selected)
                save()
                displayPage(0)
                displaySelection(false)
            else
                displaySelection(true)
                if reason == "save-failed" then self:ui_text("ambient-status", "Purchase could not be saved") end
            end
            return
        end
    end
    if accessFor(selected) ~= "open" then return end
    local row = state[selected]
    if cityWindow and sounds[selected][1] == city_sound_id and self:pressed("ambient-city-window") then
        if cityWindow:set(not cityWindow:active()) then
            displaySelection(true)
        else
            self:ui_text("ambient-status", "Window setting could not be saved")
        end
        return
    elseif cityWindow and sounds[selected][1] == city_sound_id and self:pressed("ambient-city-combined") then
        local enabled = not (cityWindow:active() or not row.muted)
        if not cityWindow:set(enabled) then
            self:ui_text("ambient-status", "Window setting could not be saved")
            return
        end
        if enabled and row.volume == 0 then row.volume = 50 end
        row.muted = not enabled
    elseif self:pressed("ambient-mute") then
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
