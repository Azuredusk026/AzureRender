local slot = "room-window-settings"
local frameRateSlot = "room-frame-rates"
local vsyncSlot = "room-vsync"
local component = "azure.window-presentation"
local modes = {"Windowed", "Borderless"}
local resolutions = {"1280 × 720", "1600 × 900", "1920 × 1080", "Custom"}
local frameRateFields = {"display-frame-active", "display-frame-inactive"}
local frameRateProperties = {"activeFrameRate", "inactiveFrameRate"}
local frameRateDefaults = {60, 24}
local window, open, saved, displayed, displayedLocale, menuCaption
local frameRates, displayedVsync

local function current()
    return {1, window:get(component, "mode"), window:get(component, "resolution"),
        window:get(component, "alwaysOnTop")}
end

local function same(a, b)
    return a and b and a[2] == b[2] and a[3] == b[3] and a[4] == b[4]
end

local function label(name, fallback)
    return display_labels and display_labels[name][displayedLocale] or fallback
end

local function show(value)
    local locale = window:get(component, "locale") or 0
    if locale ~= displayedLocale then
        displayedLocale = locale
        self:ui_text("display-toggle", label("display", "Display"))
        self:ui_text("display-mode-label", label("mode", "Window mode"))
        self:ui_text("display-resolution-label", label("resolution", "Resolution"))
        self:ui_text("display-topmost-label", label("topmost", "Always on Top"))
        self:ui_text("display-frame-active-label", label("activeRate", "When Active"))
        self:ui_text("display-frame-inactive-label", label("inactiveRate", "When Inactive"))
        self:ui_text("display-vsync-label", label("vsync", "V-Sync"))
        displayedVsync = nil
        displayed = nil
    end
    local caption = label("topmost", "Always on Top") .. "("
        .. (value[4] and label("off", "Off") or label("on", "On")) .. ")"
    if caption ~= menuCaption then
        window:set(component, "menuCaption", caption)
        menuCaption = caption
    end
    if not open then return end
    local vsync = window:get(component, "vsync") == true
    if displayedVsync ~= vsync then
        self:ui_text("display-vsync", vsync and label("on", "On") or label("off", "Off"))
        displayedVsync = vsync
    end
    if same(displayed, value) then return end
    self:ui_text("display-mode", label(value[2] == 0 and "windowed" or "borderless", modes[value[2] + 1]))
    local resolution = value[3] == 3 and label("custom", resolutions[4]) or resolutions[value[3] + 1]
    self:ui_text("display-resolution", resolution)
    self:ui_visible("display-resolution-row", value[2] == 0)
    self:ui_text("display-topmost", value[4] and label("on", "On") or label("off", "Off"))
    displayed = value
end

local function persist(key, value)
    local current = self:read_save(key)
    if type(current) == "table" and type(current[1]) == "number" and current[1] > 1 then
        self:ui_text("display-status", "Display settings require a newer version.")
        return false
    end
    if not self:try_write_save(key, value) then
        self:ui_text("display-status", "Could not save display settings.")
        return false
    end
    self:ui_text("display-status", "")
    return true
end

local function store(value)
    if not persist(slot, value) then return false end
    saved = value
    return true
end

local function apply(value)
    window:set(component, "mode", value[2])
    window:set(component, "resolution", value[3])
    window:set(component, "alwaysOnTop", value[4])
    window:set(component, "pending", true)
end

local function applyFrameRates()
    for index, property in ipairs(frameRateProperties) do
        window:set(component, property, frameRates[index])
    end
end

local function showFrameRates()
    for index, field in ipairs(frameRateFields) do
        self:ui_set_value(field, string.format("%d", frameRates[index]))
    end
end

local function validFrameRate(rate)
    return type(rate) == "number" and rate >= 10 and rate <= 250 and rate % 1 == 0
end

local function loadFrameRates()
    frameRates = {}
    for index, property in ipairs(frameRateProperties) do
        local rate = window:get(component, property)
        frameRates[index] = validFrameRate(rate) and rate or frameRateDefaults[index]
    end
    local value = self:read_save(frameRateSlot)
    if type(value) == "table" and #value == 3 and value[1] == 1
        and validFrameRate(value[2]) and validFrameRate(value[3]) then
        frameRates = {value[2], value[3]}
        applyFrameRates()
    end
end

local function editFrameRates()
    for index, field in ipairs(frameRateFields) do
        if self:pressed(field) then
            local text = self:ui_value(field)
            local number = text:match("^%s*[+%-]?%d+%s*$") and tonumber(text)
            if number then
                local changed = math.max(10, math.min(250, number))
                if changed ~= frameRates[index] then
                    local values = {frameRates[1], frameRates[2]}
                    values[index] = changed
                    if persist(frameRateSlot, {1, values[1], values[2]}) then
                        frameRates = values
                        applyFrameRates()
                    end
                end
            end
            self:ui_set_value(field, string.format("%d", frameRates[index]))
        end
    end
end

function init()
    window = assert(self:find("room-display-window"))
    open, displayed, saved = false, nil, nil
    displayedLocale, menuCaption = nil, nil
    self:ui_visible("display-toggle", true)
    self:ui_visible("display-panel", false)
    loadFrameRates()
    local vsync = self:read_save(vsyncSlot)
    if type(vsync) == "table" and #vsync == 2 and vsync[1] == 1 and type(vsync[2]) == "boolean" then
        window:set(component, "vsync", vsync[2])
    end
    local value = self:read_save(slot)
    if type(value) == "table" and #value == 4 and value[1] == 1 and modes[(tonumber(value[2]) or -1) + 1]
        and resolutions[(tonumber(value[3]) or -1) + 1] and type(value[4]) == "boolean" then
        saved = {1, tonumber(value[2]), tonumber(value[3]), value[4]}
        apply(saved)
    end
end

function update()
    local value = current()
    if saved and not same(value, saved) and not window:get(component, "pending") then store(value) end
    if open then editFrameRates() end
    local proposed
    if self:pressed("display-topmost-menu") or (open and self:pressed("display-topmost")) then
        proposed = {1, value[2], value[3], not value[4]}
    elseif self:pressed("display-toggle") then
        open = not open
        displayed = nil
        displayedVsync = nil
        self:ui_visible("display-panel", open)
        if open then showFrameRates() end
    elseif open then
        if self:pressed("display-mode") then
            proposed = {1, 1 - value[2], value[3], value[4]}
        elseif self:pressed("display-resolution") and value[2] == 0 then
            proposed = {1, value[2], (value[3] + 1) % #resolutions, value[4]}
        elseif self:pressed("display-vsync") then
            local vsync = not window:get(component, "vsync")
            if persist(vsyncSlot, {1, vsync}) then window:set(component, "vsync", vsync) end
        end
    end
    if proposed and store(proposed) then
        apply(proposed)
        value = proposed
    end
    show(value)
end
