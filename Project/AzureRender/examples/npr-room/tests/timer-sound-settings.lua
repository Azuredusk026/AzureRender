local payload, blocked
local played, stopped = {}, {}
local gain = {}
local audioAvailable = true

local host = {
    read_save = function(_, slot)
        assert(slot == "timer-sound-settings")
        return payload
    end,
    try_write_save = function(_, slot, value)
        assert(slot == "timer-sound-settings")
        if blocked then return false end
        payload = value
        return true
    end,
    find = function(_, node)
        return {
            set = function(_, component, key, value)
                assert(component == "azure.audio-source" and key == "volume")
                gain[node] = value
            end,
            audio_play = function()
                if not audioAvailable then error("Preview audio unavailable") end
                played[#played + 1] = {node, gain[node]}
            end,
            try_audio_play = function()
                if not audioAvailable then return false end
                played[#played + 1] = {node, gain[node]}
                return true
            end,
            audio_stop = function() stopped[node] = true end,
        }
    end,
}

local options = new_timer_sound_settings(host)
assert(options:play("timer-start", false))
assert(played[1][1] == "timer-start" and played[1][2] == 0.5)
assert(options:play("timer-pause", false))
assert(played[2][1] == "timer-pause" and played[2][2] == 1)
assert(not options:play("settings-click", false))

assert(options:change(false, 1, 1, 2, true))
assert(options:change(false, 1, 2, 25, true))
assert(played[#played][1] == "timer-start-b" and played[#played][2] == 0.25)
assert(stopped["timer-start"] and stopped["timer-pause"])
assert(options:change(true, 2, 1, 3, true))
assert(options:change(true, 2, 3, 1, false))
local saved = payload
blocked = true
assert(not options:change(false, 1, 1, 3, true) and payload == saved)
assert(options:play("timer-start", false))
assert(played[#played][1] == "timer-start-b" and played[#played][2] == 0.25)

options = new_timer_sound_settings(host)
assert(options:play("timer-start", true))
assert(played[#played][1] == "timer-start" and played[#played][2] == 0.5)
assert(options:play("timer-finish", true))
assert(played[#played][1] == "timer-finish-c" and played[#played][2] == 0)
assert(options:play("timer-finish", false))
assert(played[#played][1] == "timer-finish" and played[#played][2] == 0.5)
blocked = false
assert(not options:change(false, 1, 2, 101, true))
assert(not options:change(false, 1, 2, 0/0, true))
assert(payload == saved)

payload = {2, {1, 50, 0, 1, 50, 0}, {1, 50, 0, 1, 50, 0}}
options = new_timer_sound_settings(host)
assert(not options:change(false, 1, 1, 2, true))
assert(payload[1] == 2)

local actions, inputs, visible = {}, {}, {}
timer_sound_labels = {
    title = "Timer Sound", pomodoro = "Pomodoro Sound", countup = "Countup Sound",
    start = "Start Sound", finish = "End Sound", on = "On", off = "Off", error = "An error occurred",
}
host.pressed = function(_, name) return actions[name] end
host.ui_text = function() end
host.ui_visible = function(_, name, value) visible[name] = value end
host.ui_value = function(_, name) return inputs[name] end
host.ui_set_value = function(_, name, value) inputs[name] = value end

payload = nil
options = new_timer_sound_settings(host)
local menu = new_timer_sound_menu(host, options, function(value) return value end)
actions = {["timer-sound-toggle"] = true}
menu:update(false)
assert(visible["timer-sound-panel"])
inputs["timer-sound-start-volume"] = "27"
actions = {["timer-sound-start-volume"] = true, ["timer-sound-countup"] = true}
menu:update(false)
assert(payload and payload[2][2] == 27 and payload[3][2] == 50)
assert(inputs["timer-sound-start-volume"] == "50")

audioAvailable = false
assert(not options:change(false, 1, 2, 31, true), "Unavailable preview must report a recoverable failure")
assert(payload[2][2] == 31, "Preview failure must preserve the saved preference")
assert(options:play("timer-start", false), "An unavailable cue remains handled")
actions = {["timer-sound-toggle"] = true}
menu:update(false)
assert(not visible["timer-sound-panel"], "Unavailable menu cues must leave controls operable")
audioAvailable = true
assert(options:change(false, 1, 2, 31, true), "The same saved selection can be previewed after recovery")
assert(played[#played][2] == .31)

payload = {2, {"future sound preferences"}}
local protected = payload
assert(not options:change(false, 1, 1, 3, true) and payload == protected,
       "A future payload installed during the session must remain intact")
options = new_timer_sound_settings(host)
payload = nil
assert(options:change(false, 1, 1, 3, true), "Compatible payload recovery must work in the same session")
assert(payload[1] == 1 and payload[2][1] == 3)
