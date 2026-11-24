local tutorial, first = table.unpack(room_story_chapters)
assert(tutorial.group == "tutorial_01" and #tutorial.commands == 90)
assert(first.group == "main_01" and #first.commands == 20 and first.unlockLevel == 2)

local function replay(finalChoice)
    local save = {1, {}, {"City"}, {}}
    local host = {
        read_save = function() return save end,
        try_write_save = function(_, _, value) save = value; return true end,
    }
    local progress = new_story_progress(host, room_story_chapters)
    local transcript, effects, ready = {}, {}, {}
    local current, menuVisits, scriptVisits
    local topics = {1, 1, 2, 3, 4}
    local driver = {
        begin = function(_, chapter) transcript[chapter.group] = {}; current, menuVisits = chapter.group, 0 end,
        motion = function() end,
        text = function(_, row)
            assert(row.frames > 0 and row.sampleRate > 0 and row.asset:match("^voice/"))
            assert(row.translations.ZhHans and row.translations.Ko and row.translations.En)
            transcript[current][#transcript[current] + 1] = row.row
        end,
        textReady = function() return true end,
        reveal = function() error("All source text is ready in this command replay") end,
        clear = function() end,
        choices = function(_, rows, used)
            if rows[1].row == 43 then ready[1] = 1
            elseif rows[1].row == 53 then
                assert(#rows == 4 and rows[4].usedKey == 5)
                menuVisits = menuVisits + 1
                if menuVisits > 1 then assert(used[1]) end
                if menuVisits == 5 then assert(used[2] and used[3] and not used[5]) end
                ready[1] = assert(topics[menuVisits])
            else
                assert(rows[1].row == 77 and #rows == 2)
                ready[1] = finalChoice
            end
        end,
        effect = function(_, row, delay)
            effects[row.row] = delay
            if row.command:match("^TutorialFocus") then
                local waited = false
                return function() if waited then return true end; waited = true; return false end
            end
        end,
        finish = function(_, chapter, talkLog, completed)
            assert(completed and not talkLog and chapter.group == save[2][#save[2]])
        end,
    }
    local story = new_room_story(driver, progress)

    local function play(chapter, level)
        assert(story:start(chapter, level, false))
        for _ = 1, 500 do
            local state = story:state()
            if state == "idle" then return end
            local input
            if state == "script" then
                scriptVisits = scriptVisits + 1
                input = "script"
            elseif state == "choice" then input = ready[1]
            elseif state == "text" then input = "next" end
            story:update(1 / 4, input)
        end
        error("Opening route did not reach its source end")
    end

    scriptVisits = 0
    assert(progress:nextChapter(1) == tutorial)
    play(tutorial, 1)
    assert(menuVisits == 5 and scriptVisits == 1)
    assert(effects[0] == nil and effects[7] == 0 and effects[12] == .7 and effects[14] == .7)
    assert(effects[58] == 0 and effects[63] == 0 and effects[68] == 0)
    assert(table.concat(transcript[tutorial.group], ",") ==
        "9,24,30,39,44,45,46,48,57,59,57,59,62,64,67,69,72,73,74,75,76,"
        .. (finalChoice == 1 and "80" or "83") .. ",85,86")
    assert(not progress:nextChapter(1) and progress:nextChapter(2) == first)
    play(first, 2)
    assert(effects[90] == 0 and effects[91] == 0 and effects[109] == 0)
    assert(table.concat(transcript[first.group], ",") == "96,97,98,99,100,101,105,106,107,108")
    assert(save[2][1] == tutorial.group and save[2][2] == first.group and save[3][1] == "City")
end

replay(1)
replay(2)
