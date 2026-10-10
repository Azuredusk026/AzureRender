function update(dt)
    if self:pressed('restart') then self:load_level('assets:/exploration.azurelevel') end
    local collected=self:get('azure.task-state','collected')
    if self:get('azure.task-state','completed') then self:ui_text('objective','Expedition complete! Press R to explore again.')
    elseif self:get('azure.task-state','doorOpened') then self:ui_text('objective','Enter the illuminated hall.')
    elseif self:get('azure.task-state','started') then self:ui_text('objective','Find three artifacts by the golden stone markers.')
    else self:ui_text('objective','Talk to the guide at the trailhead [E].') end
    self:ui_text('progress','Artifacts: '..collected..' / 3')
    local id=self:interaction_target();local target=id~='' and self:find(id) or nil
    self:ui_text('prompt',target and target:get('azure.interactable','prompt') or '')
    local checkpoint=self:find('checkpoint:body')
    self:ui_text('checkpoint',checkpoint and checkpoint:get('azure.checkpoint','activated') and 'Checkpoint reached' or '')
    self:ui_text('route','Follow the sand trail and golden landmarks to the blue gate.')
end
