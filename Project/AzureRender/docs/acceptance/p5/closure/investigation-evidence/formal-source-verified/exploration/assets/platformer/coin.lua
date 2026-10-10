function init() self.elapsed=0 end
function update(dt)
    self.elapsed=self.elapsed+dt
    self:set('azure.transform','rotation',{0,self.elapsed*90,0})
end
function trigger(actor,entered)
    local player=self:find(actor)
    if entered and player and player:has('azure.task-state') and not self:get('azure.collectible','collected') then
        self:set('azure.collectible','collected',true)
        player:set('azure.task-state','collected',player:get('azure.task-state','collected')+1)
        local audio=self:find('audio-coin'); if audio then audio:audio_play() end
        player:ui_text('feedback','Coin collected!')
        self:destroy()
    end
end
