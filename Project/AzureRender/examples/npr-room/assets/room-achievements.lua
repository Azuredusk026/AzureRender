local tutorial = "COMPLETE_TUTORIAL_COUNT"
local hours = "POMODORO_WORK_HOURS_COUNT"
local penguin = "CLICK_KOU_COUNT"
local replayable = {
    "COMPLETE_TODO_COUNT", "COMPLETE_POMODORO_COUNT", "END_EDIT_NOTE_COUNT", "END_EDIT_DIARY_COUNT",
    "IMPORT_MUSIC_COUNT", "CHANGE_ENVIROMENT_COUNT", "FINISH_GAME_FROM_EXIT_CALL_BUTTON_COUNT",
    "TALK_WORKING_COUNT", "TALK_BREAKING_COUNT", "TALK_AFTER_WORK_COUNT", "DRINK_HOT", "WAKE_UP_HEROINE",
}
local smalltalk = {
    "SMALL_TALK_HEATSTROKE", "SMALL_TALK_GALAXY_EXPRESS", "SMALL_TALK_CAT_HEADPHONE", "SMALL_TALK_CHICADA",
}
local mainStory = {{33, "READ_GLASSES_STORY"}, {34, "PUBLISH_SUMMERTIME_OVERDRIVE"}, {36, "READ_ALL_MAIN_STORY"}}
local onceKeys = {[tutorial] = true}
for _, key in ipairs(replayable) do onceKeys[key] = true end
for _, key in ipairs(smalltalk) do onceKeys[key] = true end
for _, row in ipairs(mainStory) do onceKeys[row[2]] = true end

local function stat_integer(value)
    return type(value) == "number" and value >= -2147483648 and value <= 2147483647 and value % 1 == 0
end

function new_platform_achievement_stats(platform)
    local stats, cache = {}, {}
    local function read(key)
        local value = platform:get_stat(key)
        if not stat_integer(value) then return nil end
        cache[key] = value
        return value
    end
    local function write(key, value)
        if not platform:set_stat(key, value) then return -1 end
        cache[key] = value
        platform:store_stats()
        return value
    end
    function stats:progress(key)
        if cache[key] ~= nil then return cache[key] end
        return read(key)
    end
    function stats:set_progress(key, value)
        if not platform:initialized() or not stat_integer(value) then return -1 end
        return write(key, math.max(0, value))
    end
    function stats:increment(key, count)
        if count == nil then count = 1 end
        if not platform:initialized() or not stat_integer(count) then return -1 end
        local prior = read(key)
        if prior == nil then return -1 end
        return write(key, math.max(0, math.min(2147483647, prior + count)))
    end
    return stats
end

function new_room_achievements(host, stats)
    local room, slot = {}, "room-achievements"
    local function read()
        local save = host:read_save(slot)
        if save == nil then return {} end
        if type(save) ~= "table" or save[1] ~= 1 or type(save[2]) ~= "table" then return nil end
        local entries, seen = {}, {}
        for _, key in ipairs(save[2]) do
            if not onceKeys[key] or seen[key] then return nil end
            entries[#entries + 1], seen[key] = key, true
        end
        return entries
    end
    local function send(key)
        local prior = stats:progress(key)
        if prior ~= nil and prior ~= 1 then stats:set_progress(key, 1) end
    end
    function room:once(key)
        assert(onceKeys[key], "Unknown one-time room achievement")
        local entries = read()
        if not entries then return false end
        for _, recorded in ipairs(entries) do
            if recorded == key then
                send(key)
                return true
            end
        end
        entries[#entries + 1] = key
        if not host:try_write_save(slot, {1, entries}) then return false end
        send(key)
        return true
    end
    function room:work_time(totalSeconds)
        if type(totalSeconds) ~= "number" or totalSeconds < 0 then return end
        local value = math.floor(totalSeconds / 3600)
        if not stat_integer(value) then return end
        local prior = stats:progress(hours)
        if prior ~= nil and prior < value then stats:set_progress(hours, value) end
    end
    function room:penguin()
        local prior = stats:progress(penguin)
        if prior ~= nil and prior ~= 5 then stats:increment(penguin) end
    end
    function room:restore(tutorialComplete, groups, episode, totalSeconds)
        if tutorialComplete then send(tutorial) end
        for index, key in ipairs(smalltalk) do
            for _, group in ipairs(groups) do
                if group == "smalltalk_0" .. index then send(key); break end
            end
        end
        for _, row in ipairs(mainStory) do
            if episode >= row[1] then send(row[2]) end
        end
        self:work_time(totalSeconds)
        local entries, recorded = read(), {}
        for _, key in ipairs(entries or {}) do recorded[key] = true end
        for _, key in ipairs(replayable) do
            if recorded[key] then send(key) end
        end
    end
    return room
end
