function new_room_window(host, name, node, available, initial)
    local slot = "room-windows"
    local window, active = {}, false
    local renderable = assert(host:find(node))

    local function read()
        local saved = host:read_save(slot)
        if saved == nil then return {} end
        if type(saved) ~= "table" or saved[1] ~= 1 or type(saved[2]) ~= "table" then return nil end
        local result = {}
        for _, row in ipairs(saved[2]) do
            if type(row) ~= "table" or type(row[1]) ~= "string" or type(row[2]) ~= "boolean"
                or result[row[1]] ~= nil then return nil end
            result[row[1]] = row[2]
        end
        return result
    end

    local function present(value)
        active = value
        renderable:set("azure.renderable", "visible", value)
    end

    function window:active()
        return active
    end

    function window:restore()
        local saved = read()
        local preferred = saved and saved[name]
        if preferred == nil then preferred = initial() end
        present(available() and preferred == true)
    end

    function window:set(value)
        if not available() then return false end
        local current = read()
        if not current then return false end
        current[name] = value
        local rows = {}
        for key, visible in pairs(current) do rows[#rows + 1] = {key, visible} end
        table.sort(rows, function(a, b) return a[1] < b[1] end)
        if not host:try_write_save(slot, {1, rows}) then return false end
        present(value)
        return true
    end

    window:restore()
    return window
end
