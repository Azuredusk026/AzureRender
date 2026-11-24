local autoplay = false

function init()
    self:set("azure.character", "speed", 3)
end

function update(dt)
    local x = 0
    local z = 0
    if autoplay or self:action("move-right") then x = x + 1 end
    if self:action("move-left") then x = x - 1 end
    if self:action("move-forward") then z = z - 1 end
    if self:action("move-back") then z = z + 1 end
    self:set("azure.animator", "state", (x ~= 0 or z ~= 0) and "run" or "idle")
    self:ui_text("status", "Level ready - " .. self:get("azure.animator", "state"))
    self:move(x, z, self:pressed("jump"))
end
