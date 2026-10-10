function interact(actor)
    self:set('azure.interactable','prompt','Selected by '..actor)
    self:set('azure.interactable','enabled',false)
end
