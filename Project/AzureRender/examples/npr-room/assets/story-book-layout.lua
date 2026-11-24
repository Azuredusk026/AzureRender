function new_story_book_layout(host, layouts, default, unlockGroup, mainCategory)
    local slot = "room-book-layout"
    local books, members, nodes, prescanned = {}, {}, {}, {}
    local startsMotion = {Text = true, ChangeMotion = true, AddTextLoop = true, DirectionGameStartCamera = true}
    for name, layout in pairs(layouts) do
        local selected = {}
        for _, node in ipairs(layout) do
            nodes[node] = nodes[node] or assert(host:find(node))
            selected[node] = true
        end
        members[name] = selected
    end
    assert(members[default])

    local function present(name)
        for id, node in pairs(nodes) do
            node:set("azure.renderable", "visible", members[name][id] == true)
        end
    end

    local function read()
        local saved = host:read_save(slot)
        if saved == nil then return default end
        if type(saved) == "table" and saved[1] == 1 and members[saved[2]] then return saved[2] end
    end

    local function stillLocked()
        local progress = host:read_save("room-unlocks")
        if progress == nil then return true end
        if type(progress) ~= "table" or progress[1] ~= 1 or type(progress[2]) ~= "table" then return false end
        for _, group in ipairs(progress[2]) do
            if group == unlockGroup then return false end
        end
        return true
    end

    local function layoutFor(row)
        if row.command ~= "ChangeLayout" then return end
        local name = row.arguments[1]
        if members[name] then return name end
    end

    function books:begin(chapter, replay)
        prescanned = {}
        -- The route gate opens only the next unfinished chapter outside replay.
        local persist = not replay and chapter.category == mainCategory and stillLocked()
        for _, row in ipairs(chapter.commands) do
            if startsMotion[row.command] then break end
            local name = layoutFor(row)
            if name then
                prescanned[row] = true
                if persist and read() then host:try_write_save(slot, {1, name}) end
                present(name)
            end
        end
    end

    function books:effect(row)
        if prescanned[row] then return end
        local name = layoutFor(row)
        if name then present(name) end
    end

    function books:finish()
        prescanned = {}
        present(read() or default)
    end

    books:finish()
    return books
end
