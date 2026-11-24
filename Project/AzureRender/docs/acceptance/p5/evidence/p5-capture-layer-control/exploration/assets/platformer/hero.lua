function init()
    self.elapsed=0; self.groundedBefore=false; self.lastSound=-1; self.deaths=0
end
function sound(name)
    local source=self:find('audio-'..name)
    if source then source:audio_play() end
end
function update(dt)
    self.elapsed=self.elapsed+dt
    if self:pressed('restart') then self:load_level('assets:/platformer/platformer.azurelevel'); return end
    local p=self:get('azure.transform','translation')
    if p[2]<-5 then
        self:set('azure.transform','translation',self:get('azure.checkpoint','position'))
        self.deaths=self.deaths+1; sound('respawn'); self.lastSound=self.elapsed
        self:ui_text('feedback','Fell! Returned to checkpoint. Retry immediately.')
    end
    local grounded=self:grounded(); local velocity=self:velocity()
    if self.elapsed-self.lastSound>.18 then
        if self.groundedBefore and not grounded and velocity[2]>1 then sound('jump'); self.lastSound=self.elapsed
        elseif not self.groundedBefore and grounded then sound('land'); self.lastSound=self.elapsed end
    end
    self.groundedBefore=grounded
    local complete=self:get('azure.task-state','completed')
    self:ui_text('objective',complete and 'Course complete! Press R to play again.' or 'Follow the golden coins to the blue finish gate.')
    self:ui_text('progress','Coins: '..self:get('azure.task-state','collected')..' / 10')
    self:ui_text('checkpoint',self:get('azure.checkpoint','activated') and 'Checkpoint saved | Falls: '..self.deaths or 'Start checkpoint | Falls: '..self.deaths)
end
