function update(dt)
    if self:pressed('restart') then self:load_level('assets:/exploration.azurelevel') end
    if self:get('azure.task-state','completed') then
        self:ui_text('objective','Expedition complete! Press R to play again.')
    elseif self:get('azure.task-state','doorOpened') then
        self:ui_text('objective','Enter the hall and reach the goal.')
    elseif self:get('azure.task-state','started') then
        self:ui_text('objective','Collect three artifacts along the trail, then open the gate.')
    else
        self:ui_text('objective','Talk to the guide near the starting point [E].')
    end
    self:ui_text('progress','Artifacts: '..self:get('azure.task-state','collected')..' / 3')
    local id=self:interaction_target()
    local target=id~='' and self:find(id) or nil
    self:ui_text('prompt',target and target:get('azure.interactable','prompt') or '')
    local checkpoint=self:find('checkpoint:body')
    local position=self:get('azure.transform','translation')
    if position[3] > -47 then self:ui_text('route','Follow the trail: first artifact at the left marker.')
    elseif position[3] > -127 then self:ui_text('route','Second artifact: right marker beyond the narrow passage.')
    elseif position[3] > -207 then self:ui_text('route','Third artifact: left marker beyond the checkpoint and ramp.')
    else self:ui_text('route','Return to the trail center, open the gate and enter the hall.') end
    if checkpoint and checkpoint:get('azure.checkpoint','activated') then self:ui_text('checkpoint','Checkpoint reached') end
    if position[3] < -325 and self:get('azure.task-state','completed') then self:ui_text('route','Use the restart button or press R.') end
end
