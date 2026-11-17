local create_room_language = new_room_language

function new_room_language(config)
    local language = create_room_language(config)
    local display = self:find("room-display-window")
    if not display then return language end

    local function sync()
        local value = self:read_save("room-language")
        local index = config.default
        if type(value) == "table" and #value == 2 and value[1] == 1 and config.strings.language[value[2]] then
            index = value[2]
        end
        display:set("azure.window-presentation", "locale", index)
    end

    local update = language.update
    function language:update()
        if not update(self) then return false end
        sync()
        return true
    end

    sync()
    return language
end
