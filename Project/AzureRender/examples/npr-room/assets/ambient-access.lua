function new_ambient_access(wallet)
    local reader = self
    local played, purchases, timestamp
    local access = {}
    local function listed(values)
        local set = {}
        if type(values) == "table" then
            for _, name in ipairs(values) do
                if type(name) == "string" then set[name] = true end
            end
        end
        return set
    end

    function access:refresh()
        local saved = reader:read_save("room-unlocks")
        if type(saved) ~= "table" or saved[1] ~= 1 then saved = {} end
        played, purchases = listed(saved[2]), listed(saved[3])
        if wallet then wallet:refresh() end
        timestamp = reader:local_datetime()
    end

    function access:state(id)
        local item = environment_access[id]
        local policy, argument, expires = item[1], item[2], item[3]
        if policy == "always" or (policy == "scenario" and played[argument])
            or (policy == "purchase" and (purchases[item[4]] or (wallet and wallet:owns(item[4])))) then
            return "open"
        end
        if expires ~= "" and timestamp > expires then return "expired" end
        local hint = policy == "purchase" and ("Requires purchase · " .. argument .. " points")
            or "Unlocks through the story"
        return "locked", hint
    end

    access:refresh()
    return access
end
