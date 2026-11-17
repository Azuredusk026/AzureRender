function new_story_player(host, chapters, dialogue, currentLanguage, currentLevel, presentation, labels)
    local progress = new_story_progress(host, chapters)
    local achievements = new_room_achievements and new_room_achievements(host)
    if achievements then
        for _, chapter in ipairs(chapters) do
            if chapter.category == 18 and progress:canStart(chapter, 1, true) then
                achievements:once("COMPLETE_TUTORIAL_COUNT")
            end
        end
    end
    local driver = {}
    local player = {}
    local options, seen, automatic, logOpen
    local lastLevel, lastAvailable, lastState
    local replayEntries = {}
    local prefix = "opening-story-"
    local optionCount = 4

    local function visible(id, value)
        host:ui_visible(prefix .. id, value)
    end

    local function title(id, chapter)
        host:ui_text(prefix .. id, assert(chapter.title[currentLanguage()]))
    end

    local function localizeControls()
        for id, values in pairs(labels) do host:ui_text(prefix .. id, assert(values[currentLanguage()])) end
    end

    local function showChoices()
        local language = currentLanguage()
        for index = 1, optionCount do
            local option = options[index]
            visible("option-" .. index, option ~= nil)
            visible("option-used-" .. index, option and option.usedKey and seen[option.usedKey] == true or false)
            if option then host:ui_text(prefix .. "option-label-" .. index, assert(option.translations[language])) end
        end
        visible("options", true)
    end

    function driver:clear()
        dialogue:clear()
        visible("options", false)
        options, seen = nil, nil
    end

    function driver:begin(chapter, replay)
        automatic, logOpen = true, false
        lastState = nil
        visible("start", false)
        visible("log", false)
        visible("log-panel", false)
        presentation:begin(chapter, replay)
    end

    function driver:finish(chapter, replay, completed)
        self:clear()
        presentation:finish(chapter, replay, completed)
        if achievements and completed and not replay and chapter.category == 18 then
            achievements:once("COMPLETE_TUTORIAL_COUNT")
        end
    end

    function driver:choices(rows, used)
        assert(#rows <= optionCount)
        options, seen = rows, used
        showChoices()
    end

    function driver:motion(row) presentation:motion(row) end
    function driver:effect(row, delay) return presentation:effect(row, delay) end
    function driver:text(row) dialogue:show(row) end
    function driver:textReady() return dialogue:ready() end
    function driver:reveal() dialogue:fastForward() end

    local story = new_room_story(driver, progress)

    local function refreshMenu(level, available)
        local nextChapter = available and progress:nextChapter(level)
        visible("start", nextChapter and true or false)
        if nextChapter then title("start", nextChapter) end
        replayEntries = {}
        for index, chapter in ipairs(chapters) do
            if available and progress:canStart(chapter, level, true) then
                replayEntries[index] = chapter
                title("replay-" .. index, chapter)
            end
            visible("replay-" .. index, replayEntries[index] ~= nil)
        end
        local hasLog = next(replayEntries) ~= nil
        visible("log", hasLog)
        visible("log-panel", hasLog and logOpen == true)
    end

    local function syncControls()
        local state = story:state()
        if state == lastState then return end
        lastState = state
        visible("auto", state ~= "idle" and state ~= "script")
        visible("next", state == "text" or state == "persist")
        visible("heroine", state == "script")
        if state == "idle" then lastAvailable = nil end
    end

    local function storyInput()
        local state = story:state()
        if state == "script" then
            if host:pressed(prefix .. "heroine") then return "script" end
        elseif options then
            for index in ipairs(options) do
                if host:pressed(prefix .. "option-" .. index) then return index end
            end
        end
        if host:pressed(prefix .. "auto") and state ~= "script" then
            automatic = not automatic
            visible("auto-active", automatic)
            return "auto"
        end
        if host:pressed(prefix .. "next") then return "next" end
    end

    function player:update(dt, available, languageChanged)
        presentation:update(dt)
        if languageChanged then localizeControls() end
        if story:state() == "idle" then
            local level = currentLevel()
            if level ~= lastLevel or available ~= lastAvailable or languageChanged then
                refreshMenu(level, available)
                lastLevel, lastAvailable = level, available
            end
            if not available then return false end
            if host:pressed(prefix .. "start") then
                local chapter = progress:nextChapter(level)
                if chapter then story:start(chapter, level, false, true) end
            elseif host:pressed(prefix .. "log") and next(replayEntries) then
                logOpen = not logOpen
                visible("log-panel", logOpen)
            elseif logOpen then
                for index, chapter in pairs(replayEntries) do
                    if host:pressed(prefix .. "replay-" .. index) then
                        story:start(chapter, level, true, true)
                        break
                    end
                end
            end
            if story:state() == "idle" then return false end
            visible("auto-active", automatic)
        elseif languageChanged then
            dialogue:languageChanged()
            if options then showChoices() end
        end
        dialogue:update(dt)
        story:update(dt, storyInput())
        syncControls()
        return true
    end

    localizeControls()
    syncControls()
    return player
end
