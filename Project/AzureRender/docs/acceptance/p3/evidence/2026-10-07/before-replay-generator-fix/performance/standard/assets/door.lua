function interact(actor)
    local player=self:find(actor)
    if not player or not player:has('azure.task-state') then return end
    if self:get('azure.door','open') or player:get('azure.task-state','collected')<self:get('azure.door','requiredCount') then return end
    self:set('azure.door','open',true)
    self:set('azure.interactable','enabled',false)
    player:set('azure.task-state','doorOpened',true)
    self:remove_component('azure.rigid-body')
    self:set('azure.renderable','visible',false)
end
