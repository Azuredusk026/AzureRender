function interact(actor)
    local player=self:find(actor)
    if not player or not player:has('azure.task-state') then return end
    player:set('azure.task-state','started',true)
    self:set('azure.interactable','enabled',false)
end
