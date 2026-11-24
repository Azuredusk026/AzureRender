function interact(actor)
    local player=self:find(actor)
    if not player or not player:has('azure.task-state') or not player:get('azure.task-state','started') then return end
    if self:get('azure.collectible','collected') then return end
    self:set('azure.collectible','collected',true)
    self:set('azure.interactable','enabled',false)
    player:set('azure.task-state','collected',player:get('azure.task-state','collected')+1)
    self:destroy()
end
