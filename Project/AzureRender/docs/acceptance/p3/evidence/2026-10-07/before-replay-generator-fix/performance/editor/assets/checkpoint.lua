function trigger(actor,entered)
    local player=self:find(actor)
    if entered and player and player:has('azure.task-state') then self:set('azure.checkpoint','activated',true) end
end
