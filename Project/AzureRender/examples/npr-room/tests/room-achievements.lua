local tutorial = "COMPLETE_TUTORIAL_COUNT"
local notes = "END_EDIT_NOTE_COUNT"
local todo = "COMPLETE_TODO_COUNT"
local hours = "POMODORO_WORK_HOURS_COUNT"
local kou = "CLICK_KOU_COUNT"

local durable = {}
local platform = {online = false, read = true, write = true, store = true}
local pending, calls = {}, {}
function platform:initialized() return self.online end
function platform:get_stat(key)
    calls[#calls + 1] = "read:" .. key
    if not self.online or not self.read then return nil end
    return pending[key] or durable[key] or 0
end
function platform:set_stat(key, progress)
    calls[#calls + 1] = "write:" .. key
    if not self.online or not self.write then return false end
    pending[key] = progress
    return true
end
function platform:store_stats()
    calls[#calls + 1] = "store"
    if not self.store then return false end
    for key, value in pairs(pending) do durable[key] = value end
    pending = {}
    return true
end

local stats = new_platform_achievement_stats(platform)
assert(stats:set_progress(notes, 1) == -1 and stats:increment(notes) == -1 and #calls == 0)
assert(stats:progress(notes) == nil and #calls == 1)
platform.online, platform.read = true, false
assert(stats:increment(notes) == -1 and calls[#calls] == "read:" .. notes and #calls == 2)
platform.read, platform.write = true, false
durable[notes] = 4
assert(stats:increment(notes, 2) == -1 and calls[#calls] == "write:" .. notes and #calls == 4)
assert(stats:progress(notes) == 4 and #calls == 4)
platform.write = true
assert(stats:increment(notes, -9) == 0 and durable[notes] == 0 and calls[#calls] == "store")
assert(stats:set_progress(notes, -6) == 0 and durable[notes] == 0)
platform.store = false
assert(stats:set_progress(notes, 7) == 7 and stats:progress(notes) == 7 and durable[notes] == 0)
pending = {}
stats = new_platform_achievement_stats(platform)
assert(stats:progress(notes) == 0)

local save, writes = nil, 0
local host = {writable = true}
function host:read_save(slot)
    assert(slot == "room-achievements")
    return save
end
function host:try_write_save(slot, value)
    assert(slot == "room-achievements")
    writes = writes + 1
    if not self.writable then return false end
    save = value
    return true
end

platform.store, platform.online = true, false
local room = new_room_achievements(host, new_platform_achievement_stats(platform))
assert(room:once(todo) and room:once(todo) and writes == 1)
assert(save[1] == 1 and #save[2] == 1 and save[2][1] == todo and durable[todo] == nil)
host.writable = false
assert(not room:once(notes) and #save[2] == 1 and writes == 2)
platform.online, host.writable = true, true
room = new_room_achievements(host, new_platform_achievement_stats(platform))
room:restore(true, {"smalltalk_01", "smalltalk_03"}, 34, 3 * 3600 + 3599)
assert(durable[tutorial] == 1 and durable[todo] == 1 and durable[hours] == 3)
assert(durable.SMALL_TALK_HEATSTROKE == 1 and durable.SMALL_TALK_CAT_HEADPHONE == 1)
assert(durable.SMALL_TALK_CHICADA == nil and durable.READ_GLASSES_STORY == 1
       and durable.PUBLISH_SUMMERTIME_OVERDRIVE == 1 and durable.READ_ALL_MAIN_STORY == nil)
assert(writes == 2, "Restore uses progress from existing saves")
room:work_time(3599)
assert(durable[hours] == 3)
room:work_time(4 * 3600)
assert(durable[hours] == 4)
for _ = 1, 7 do room:penguin() end
assert(durable[kou] == 5)
assert(room:once(notes) and save[2][1] == todo and save[2][2] == notes and writes == 3)
