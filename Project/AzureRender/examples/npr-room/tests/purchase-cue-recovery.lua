local function read(name)
    local file = assert(io.open("examples/npr-room/assets/"..name,"r"))
    local text=file:read("*a");file:close();return text
end
local saved, sources, actions = {}, {}, {}
local available=false
local host={}
function host:read_save(slot) return saved[slot] end
function host:try_write_save(slot,value) saved[slot]=value;return true end
function host:local_datetime() return "20261124010000" end
function host:ui_visible() end
function host:ui_text() end
function host:pressed(action) return actions[action] end
function host:find(name)
    if not sources[name] then
        local source={starts=0}
        function source:set() end
        function source:audio_play()
            if name=="room-buy-item" and not available then error("Purchase cue unavailable") end
            self.starts=self.starts+1
        end
        function source:try_audio_play()
            if name=="room-buy-item" and not available then return false end
            self:audio_play();return true
        end
        sources[name]=source
    end
    return sources[name]
end
local config=[[
local sounds={{1,"first","assets:/one.wav","Original"},{2,"second","assets:/two.wav","Another"}}
local wallet_catalog={Original={0,""},Another={0,""}}
local environment_access={[1]={"purchase",0,"","Original"},[2]={"purchase",0,"","Another"}}
]]
local env=setmetatable({self=host},{__index=_G})
assert(load(config..read("room-wallet.lua")..read("ambient-access.lua")..read("ambient.lua"),"@purchase-ambient","t",env))()
env.init();env.update()
local function click(name)
    actions={["ambient-"..name]=true};env.update();actions={}
end
click("toggle");click("select-1");click("purchase-confirm")
assert(saved["room-purchases"][2][1][1]=="Original")
assert(saved["ambient"][2][1][1]==1 and sources.first.starts==1,
       "Purchase cue failure must preserve ambient activation after a committed purchase")
available=true
click("select-2");click("purchase-confirm")
assert(#saved["room-purchases"][2]==2 and sources.second.starts==1 and sources["room-buy-item"].starts==1)
print("Optional purchase cue failure preserves purchase, ambient activation and recovery")
