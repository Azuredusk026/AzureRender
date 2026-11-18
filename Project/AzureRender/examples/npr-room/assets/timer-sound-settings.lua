function new_timer_sound_settings(host)
    local slot = "timer-sound-settings"
    local cueNodes = {
        {"timer-start", "timer-start-b", "timer-start-c"},
        {"timer-finish", "timer-finish-b", "timer-finish-c"},
    }
    local defaults = {1, 50, 0, 1, 50, 0}
    local lower = {1, 0, 0}
    local upper = {3, 100, 1}
    local data = host:read_save(slot)
    local writable = type(data) ~= "table" or type(data[1]) ~= "number" or data[1] <= 1

    local function valid(field, value)
        return type(value) == "number" and value >= lower[field] and value <= upper[field] and value % 1 == 0
    end

    local function validMode(value)
        if type(value) ~= "table" or #value ~= #defaults then return false end
        for index = 1, #defaults do
            if not valid((index - 1) % 3 + 1, value[index]) then return false end
        end
        return true
    end

    if type(data) ~= "table" or #data ~= 3 or data[1] ~= 1 or not validMode(data[2]) or not validMode(data[3]) then
        data = {1, defaults, defaults}
    end

    local settings = {}

    function settings:values(countup, group)
        local selected = data[countup and 3 or 2]
        return table.unpack(selected, group * 3 - 2, group * 3)
    end

    local function playNode(name, gain)
        local cue = assert(host:find(name), "Missing timer sound: " .. name)
        cue:set("azure.audio-source", "volume", gain)
        cue:audio_play()
    end

    local function playGroup(countup, group)
        local choice, volume, mute = settings:values(countup, group)
        playNode(cueNodes[group][choice], mute == 1 and 0 or volume / 100)
    end

    function settings:play(id, countup)
        for group, nodes in ipairs(cueNodes) do
            if id == nodes[1] then
                playGroup(countup, group)
                return true
            end
        end
        if id == "timer-pause" then
            playNode(id, 1)
            return true
        end
        return false
    end

    function settings:change(countup, group, field, value, preview)
        if not writable or not valid(field, value) then return false end
        local mode = countup and 3 or 2
        local updatedMode = {table.unpack(data[mode])}
        local index = (group - 1) * 3 + field
        if updatedMode[index] ~= value then
            updatedMode[index] = value
            local updated = {table.unpack(data)}
            updated[mode] = updatedMode
            if not host:try_write_save(slot, updated) then return false end
            data = updated
        end
        if preview then
            host:find("timer-pause"):audio_stop()
            for _, nodes in ipairs(cueNodes) do
                for _, name in ipairs(nodes) do host:find(name):audio_stop() end
            end
            playGroup(countup, group)
        end
        return true
    end

    return settings
end
