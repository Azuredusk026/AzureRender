function new_room_wallet(host, catalog)
    local wallet, entries, spent, owned = {}, nil, 0, {}
    local slot = "room-purchases"

    local function earnedPoints()
        local value = host:read_save("work-calendar")
        local growth = type(value) == "table" and value[1] == 2 and value[3]
        local points = type(growth) == "table" and tonumber(growth[3])
        return points and math.tointeger(points) and math.max(0, points) or 0
    end

    function wallet:refresh()
        local document = host:read_save(slot)
        local current, used, seen = {}, 0, {}
        if document ~= nil then
            if type(document) ~= "table" or document[1] ~= 1 or type(document[2]) ~= "table" then
                entries, spent, owned = nil, 0, {}
                return false
            end
            for _, row in ipairs(document[2]) do
                local rule = type(row) == "table" and catalog[row[1]]
                if not rule or row[2] ~= rule[1] or seen[row[1]] then
                    entries, spent, owned = nil, 0, {}
                    return false
                end
                current[#current + 1] = row
                seen[row[1]], used = true, used + rule[1]
            end
        end
        local grants = host:read_save("room-unlocks")
        if type(grants) == "table" and grants[1] == 1 and type(grants[3]) == "table" then
            for _, name in ipairs(grants[3]) do
                if type(name) == "string" then seen[name] = true end
            end
        end
        entries, spent, owned = current, used, seen
        return true
    end

    function wallet:balance(gross)
        return math.max(0, (gross or earnedPoints()) - spent)
    end

    function wallet:owns(name)
        return owned[name] == true
    end

    function wallet:offer(name)
        local rule = catalog[name]
        if not rule or self:owns(name) or (rule[2] ~= "" and host:local_datetime() > rule[2]) then return end
        return rule[1], entries ~= nil and self:balance() >= rule[1]
    end

    function wallet:purchase(name)
        if not self:refresh() then return false, "save-failed" end
        if self:owns(name) then return false, "owned" end
        local price, affordable = self:offer(name)
        if not price then return false, "unavailable" end
        if not affordable then return false, "insufficient" end
        local updated = {table.unpack(entries)}
        updated[#updated + 1] = {name, price}
        if not host:try_write_save(slot, {1, updated}) then return false, "save-failed" end
        entries, spent, owned[name] = updated, spent + price, true
        return true
    end

    wallet:refresh()
    return wallet
end
