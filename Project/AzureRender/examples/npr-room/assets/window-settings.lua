local slot = "room-window-settings"
local component = "azure.window-presentation"
local modes = {"Windowed", "Borderless"}
local resolutions = {"1280 × 720", "1600 × 900", "1920 × 1080", "Custom"}
local window, open, saved, displayed, writable

local function current()
    return {1, window:get(component, "mode"), window:get(component, "resolution"),
        window:get(component, "alwaysOnTop")}
end

local function same(a, b)
    return a and b and a[2] == b[2] and a[3] == b[3] and a[4] == b[4]
end

local function show(value)
    if not open or same(displayed, value) then return end
    self:ui_text("display-mode", modes[value[2] + 1])
    self:ui_text("display-resolution", resolutions[value[3] + 1])
    self:ui_visible("display-resolution-row", value[2] == 0)
    self:ui_text("display-topmost", value[4] and "On" or "Off")
    displayed = value
end

local function store(value)
    if not writable then
        self:ui_text("display-status", "Display settings require a newer version.")
        return false
    end
    if not self:try_write_save(slot, value) then
        self:ui_text("display-status", "Could not save display settings.")
        return false
    end
    saved = value
    self:ui_text("display-status", "")
    return true
end

local function apply(value)
    window:set(component, "mode", value[2])
    window:set(component, "resolution", value[3])
    window:set(component, "alwaysOnTop", value[4])
    window:set(component, "pending", true)
end

function init()
    window = assert(self:find("room-display-window"))
    open, displayed, saved = false, nil, nil
    self:ui_visible("display-toggle", true)
    self:ui_visible("display-panel", false)
    local value = self:read_save(slot)
    writable = type(value) ~= "table" or type(value[1]) ~= "number" or value[1] <= 1
    if type(value) == "table" and #value == 4 and value[1] == 1 and modes[(tonumber(value[2]) or -1) + 1]
        and resolutions[(tonumber(value[3]) or -1) + 1] and type(value[4]) == "boolean" then
        saved = {1, tonumber(value[2]), tonumber(value[3]), value[4]}
        apply(saved)
    end
end

function update()
    local value = current()
    if saved and not same(value, saved) and not window:get(component, "pending") then store(value) end
    if self:pressed("display-toggle") then
        open = not open
        displayed = nil
        self:ui_visible("display-panel", open)
    elseif open then
        local proposed
        if self:pressed("display-mode") then
            proposed = {1, 1 - value[2], value[3], value[4]}
        elseif self:pressed("display-resolution") and value[2] == 0 then
            proposed = {1, value[2], (value[3] + 1) % #resolutions, value[4]}
        elseif self:pressed("display-topmost") then
            proposed = {1, value[2], value[3], not value[4]}
        end
        if proposed and store(proposed) then
            apply(proposed)
            value = proposed
        end
    end
    show(value)
end
