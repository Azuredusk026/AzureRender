function trigger(actor,entered)
    local player=self:find(actor)
    if entered and player and player:has('azure.task-state') and not player:get('azure.task-state','completed') then
        player:set('azure.task-state','completed',true)
        player:set('azure.character','controlled',false)
        self:set('azure.door','open',true)
        local audio=self:find('audio-finish'); if audio then audio:audio_play() end
        player:ui_text('feedback','All platforms cleared! Optional coins remain a bonus.')
    end
end
