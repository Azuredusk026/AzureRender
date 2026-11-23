local selection = os.getenv("AZURE_VOICE_CASE") or "all"
local function read(name)
    local file = assert(io.open("examples/npr-room/assets/" .. name, "r"))
    local value = file:read("*a"); file:close(); return value
end
local function context(config, names)
    local available, action = false, nil
    local visible, text, saved = {}, {}, {}
    local source = {starts=0, playing=false, cursor=0}
    function source:set() end
    function source:audio_play()
        if not available then error("Optional voice unavailable") end
        self.starts, self.playing, self.cursor = self.starts+1, true, 0
    end
    function source:try_audio_play()
        if not available then return false end
        self:audio_play(); return true
    end
    function source:audio_stop() self.playing = false end
    function source:audio_playing() return self.playing end
    function source:audio_position() return self.cursor end
    local host = {}
    function host:find() return source end
    function host:pressed(name) return name == action end
    function host:ui_text(id, value) text[id] = value end
    function host:ui_visible(id, value) visible[id] = value end
    function host:read_save(slot) return saved[slot] end
    function host:try_write_save(slot, value) saved[slot] = value; return true end
    local env = setmetatable({self=host}, {__index=_G})
    local script = config
    for _, name in ipairs(names) do script = script .. "\n" .. read(name) end
    assert(load(script, "@production-voice", "t", env))()
    return env, source, visible, saved, function(value) available=value end, function(value) action=value end
end

if selection == "all" or selection == "room" then
    local env, source, visible, _, available, action = context(
        'local responses={{"assets:/voice.wav","Original greeting",1,0,.25}}', {"room-voice.lua"})
    local step = env.new_room_voice()
    action("room-voice-talk")
    assert(step(.01,true,false,false))
    action(nil)
    assert(step(.5,true,false,false) and visible["room-voice-caption"])
    assert(step(.5,true,false,false))
    assert(not step(.25,true,false,false) and not visible["room-voice-caption"])
    available(true); action("room-voice-talk")
    assert(step(.01,true,false,false) and source.starts==1)
    print("Room voice failure completes its caption timeline and retries")
end

if selection == "all" or selection == "choice" then
    local env, source, visible, saved, available, action = context([[
local work_choice_parameters={fullProbabilitySeconds=1,probability=1,availableSeconds=100}
local work_conversations={{{"assets:/voice.wav","Original question",1,0,.25},
    {{"Original option",{"assets:/voice.wav","Original response",1,0,.25}}}}}
]], {"work-choice.lua"})
    local story = env.new_work_choice(function(limit) return limit and 1 or 0 end)
    story:recordWork(1); story:complete(); action("room-voice-talk")
    assert(story:step(.01,true,false)); action(nil)
    story:step(1,true,false); story:step(.25,true,false)
    assert(saved["work-conversation"][3]==2 and visible["room-choice-options"])
    action("room-choice-option-1"); story:step(.01,true,false); action(nil)
    story:step(.01,true,false); story:step(1,true,false); story:step(.25,true,false)
    assert(saved["work-conversation"][2]==0 and not story:step(0,true,false))
    available(true); story:recordWork(1); story:complete(); action("room-voice-talk")
    assert(story:step(.01,true,false) and source.starts==1)
    print("Work choice voice failure preserves option progress and completion")
end

if selection == "all" or selection == "timer" then
    local env, source, visible, _, available = context("", {"timer-voice-selector.lua","timer-voice-action.lua"})
    local line={asset="assets:/voice.wav",localization="original",text="Original timer line",seconds=1,
        bodyMotion=-1,speakBeforeMotion=true,animationCue=0}
    local player=env.new_timer_voice_action({groups={[4]={line}},motions={},rules={longWorkStart={seconds=1000,percent=0}},
        random=function(limit) return limit and 1 or 0 end}, function() end, function(value) return value end)
    assert(player:trigger("work",false,1,60,true,false,"idle"))
    assert(player:update(.01,false) and player:update(.5,false))
    assert(not player:update(.5,false) and not visible["timer-voice-caption"])
    available(true)
    assert(player:trigger("work",false,1,60,true,false,"idle") and source.starts==1)
    print("Timer voice failure completes at declared duration and retries")
end
