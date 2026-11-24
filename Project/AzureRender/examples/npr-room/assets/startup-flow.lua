local room = "assets:/room.azurelevel"
local next_level = {
    startup = "assets:/asset-load.azurelevel",
    ["asset-load"] = "assets:/entry.azurelevel",
    entry = "assets:/transition.azurelevel",
}
local state
local loading_seconds
local reveal_seconds

local function begin_load()
    state = "loading"
    loading_seconds = 0
    reveal_seconds = nil
    self:preload_level(room)
end

function init()
    if self.id == "transition" then
        begin_load()
    else
        self:load_level(assert(next_level[self.id]))
    end
end

function update(dt)
    if self.id ~= "transition" then return end

    if self:pressed("transition-cancel") then
        self:cancel_level_load()
        state = "cancelled"
        self:ui_text("transition-status", "Loading cancelled. Press Enter to retry.")
    elseif self:pressed("transition-retry") and (state == "cancelled" or state == "failed") then
        begin_load()
        self:ui_text("transition-status", "Loading room. Press Backspace to cancel.")
    end
    if state ~= "loading" and state ~= "committing" then return end

    local error_message = self:level_load_error()
    if error_message ~= "" then
        state = "failed"
        self:ui_text("transition-status", error_message .. " Press Enter to retry.")
        return
    end
    if state == "committing" then return end
    loading_seconds = loading_seconds + dt
    if loading_seconds < 2 or not self:level_ready(room) then
        reveal_seconds = nil
        return
    end
    if reveal_seconds == nil then
        reveal_seconds = 0
        return
    end
    reveal_seconds = reveal_seconds + dt
    if reveal_seconds >= 0.5 then
        state = "committing"
        self:load_level(room)
    end
end
