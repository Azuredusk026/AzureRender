function new_story_room(host, chapters, tracks, speeds, labels, language, level, books, body, desk, camera, face)
    local music = new_story_music(host, tracks)
    local noise = new_story_noise(host)
    local dialogue = new_story_dialogue(host, speeds, language)
    local presentation = {}
    local focus
    local highlights = {TutorialFocusPomodoro = "opening-focus-clock",
        TutorialFocusTodo = "opening-focus-todo", TutorialFocusNote = "opening-focus-notes"}

    local function clearFocus()
        if not focus then return end
        host:ui_visible(focus.id, false)
        focus = nil
    end

    function presentation:begin(chapter, replay)
        host:ui_visible("opening-story-input-mask", true)
        if books then books:begin(chapter, replay) end
        if desk then desk:begin(chapter) end
        if body then body:begin(chapter) end
        noise:begin(chapter, replay)
        if face then face:begin(chapter, replay) end
    end

    function presentation:motion(row)
        if body then body:row(row) end
        if face then face:expression(row) end
    end

    function presentation:update(dt)
        music:update(dt)
        noise:update(dt)
        if camera then camera:update(dt) end
        if face then
            face:voice(dialogue:speaking())
            face:update(dt)
        end
        if focus then focus.remaining = focus.remaining - dt end
    end

    function presentation:effect(row, delay)
        if books then books:effect(row) end
        if body and body.effect then body:effect(row) end
        if camera then camera:effect(row) end
        noise:effect(row, delay)
        if row.command == "PlayBGM" or row.command == "TalkLogOnlyPlayBGM" then music:play(row.arguments) end
        local id = highlights[row.command]
        if not id then return end
        clearFocus()
        dialogue:clear()
        local active = {id = id, remaining = 3.5}
        focus = active
        host:ui_visible(id, true)
        return function()
            if focus ~= active then return true end
            if active.remaining > 1e-7 then return false end
            clearFocus()
            return true
        end
    end

    function presentation:finish()
        clearFocus()
        host:ui_visible("opening-story-input-mask", false)
        if books then books:finish() end
        if body then body:finish() end
        music:finish()
        noise:finish()
        if camera then camera:finish() end
        if face then face:finish() end
    end

    return new_story_player(host, chapters, dialogue, language, level, presentation, labels)
end
