function trigger(actor,entered)
    local player=self:find(actor)
    if entered and player and player:has('azure.checkpoint') then
        self:set('azure.checkpoint','activated',true)
        player:set('azure.checkpoint','activated',true)
        player:set('azure.checkpoint','position',self:get('azure.checkpoint','position'))
        player:ui_text('feedback','Checkpoint reached. Your coins are retained after a fall.')
    end
end
