function new_story_progress(host, chapters)
    local slot = "room-unlocks"
    local progress = {}
    local authored = {}
    for _, chapter in ipairs(chapters) do authored[chapter.group] = chapter end

    local function read()
        local saved = host:read_save(slot)
        if type(saved) ~= "table" then saved = {1, {}, {}, {}} end
        if saved[1] ~= 1 then return nil end
        local played = {}
        if type(saved[2]) == "table" then
            for _, identity in ipairs(saved[2]) do
                if type(identity) == "string" then played[identity] = true end
            end
        end
        return saved, played
    end

    function progress:nextChapter(level)
        local _, played = read()
        if not played then return nil end
        for _, chapter in ipairs(chapters) do
            if not played[chapter.group] then
                if level >= chapter.unlockLevel then return chapter end
                return nil
            end
        end
    end

    function progress:canStart(chapter, level, replay)
        if authored[chapter.group] ~= chapter then return false end
        if replay then
            local _, played = read()
            return played and played[chapter.group] == true
        end
        return self:nextChapter(level) == chapter
    end

    function progress:complete(chapter)
        if authored[chapter.group] ~= chapter then return false end
        local saved, played = read()
        if not saved then return false end
        if played[chapter.group] then return true end
        local ordered = {}
        for _, identity in ipairs(type(saved[2]) == "table" and saved[2] or {}) do
            if played[identity] then
                ordered[#ordered + 1] = identity
                played[identity] = nil
            end
        end
        ordered[#ordered + 1] = chapter.group
        local value = {table.unpack(saved)}
        value[2] = ordered
        return host:try_write_save(slot, value)
    end

    return progress
end
