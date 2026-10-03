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
    self:move(x, z, self:pressed("jump"))
end
