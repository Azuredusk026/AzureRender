function trigger(actor,entered)
    local player=self:find(actor)
    if entered and player and player:has('azure.task-state') and player:get('azure.task-state','collected')==3 and player:get('azure.task-state','doorOpened') then
        player:set('azure.task-state','completed',true)
    end
end
