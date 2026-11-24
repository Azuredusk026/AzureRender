local preferenceSlot = "local-media-view"
local music, pictures = {}, {}
local song, picture = 1, 1
local songId, pictureId = "", ""
local ready, playing, resume = false, false, false
local previousTask = ""

local function status(message) self:ui_text("media-status", message) end
local function savePreferences()
    local saved = self:read_save(preferenceSlot)
    if type(saved) == "table" and type(saved[1]) == "number" and saved[1] > 1 then
        status("偏好版本较新，当前选择未保存")
        return
    end
    if not self:try_write_save(preferenceSlot, {1, songId, pictureId, playing}) then
        status("选择未能保存，请重试")
    end
end

local function selected(rows, id)
    for index, row in ipairs(rows) do if row[1] == id then return index end end
    return 1
end

local function showMusic()
    local row = music[song]
    songId = row and row[1] or ""
    self:ui_text("media-music-title", row and row[2] or "尚未导入音乐")
    self:ui_text("media-music-count", string.format("%d / %d", row and song or 0, #music))
    self:ui_text("media-play", playing and "暂停" or "播放")
end

local function showPicture()
    local row = pictures[picture]
    pictureId = row and row[1] or ""
    self:ui_text("media-image-title", row and row[2] or "尚未导入图片")
    self:ui_text("media-image-count", string.format("%d / %d", row and picture or 0, #pictures))
    self:ui_visible("media-preview", row ~= nil and row[4])
    if not self:try_ui_image("media-preview", row and row[4] and row[3] or "") then status("图片暂不可用，请刷新或重新导入") end
end

local function refresh()
    local newMusic = self:try_media_entries("music")
    local newImages = self:try_media_entries("images")
    if not newMusic[1] or not newImages[1] then
        status("媒体清单暂不可用，可重新选择或重试")
        return false
    end
    music, pictures = newMusic[2], newImages[2]
    song, picture = selected(music, songId), selected(pictures, pictureId)
    showMusic(); showPicture()
    return true
end

local function play()
    local row = music[song]
    if not row or not row[4] then status("请选择可用音乐，或重新导入缺失文件"); return end
    self:audio_stop()
    self:set("azure.audio-source", "asset", row[3])
    self:set("azure.audio-source", "volume", .4)
    self:set("azure.audio-source", "loop", false)
    playing = self:try_audio_play()
    status(playing and "" or "音乐暂不可用，点击播放可重试")
    showMusic(); savePreferences()
end

local function moveSong(direction)
    if #music == 0 then return end
    local continuePlaying = playing
    if playing then self:audio_stop(); playing = false end
    song = (song - 1 + direction) % #music + 1
    showMusic()
    if continuePlaying then play() else savePreferences() end
end

local function import(kind, directory)
    self:media_import(kind, directory)
    previousTask = ""
end

function init()
    local saved = self:read_save(preferenceSlot)
    if type(saved) == "table" and saved[1] == 1 and type(saved[2]) == "string"
        and type(saved[3]) == "string" then
        songId, pictureId, resume = saved[2], saved[3], saved[4] == true
    end
end

function update()
    if not ready then
        ready = true
        refresh()
        if resume then play() end
    end
    if self:pressed("media-import-music") then import("music", false) end
    if self:pressed("media-folder-music") then import("music", true) end
    if self:pressed("media-import-images") then import("images", false) end
    if self:pressed("media-folder-images") then import("images", true) end
    if self:pressed("media-cancel") then self:media_cancel() end
    if self:pressed("media-refresh") then refresh() end
    if self:pressed("media-music-prev") then moveSong(-1) end
    if self:pressed("media-music-next") then moveSong(1) end
    if self:pressed("media-play") then
        if playing then self:audio_stop(); playing = false; showMusic(); savePreferences() else play() end
    end
    for _, direction in ipairs({-1, 1}) do
        if self:pressed(direction == -1 and "media-image-prev" or "media-image-next") and #pictures > 0 then
            picture = (picture - 1 + direction) % #pictures + 1
            showPicture(); savePreferences()
        end
    end
    local task = self:media_status()
    if task[1] == "running" then
        status(string.format("正在导入：%d / %d，失败 %d", task[2], task[3], task[4]))
    elseif task[1] ~= previousTask then
        if task[1] == "completed" then
            status(string.format("导入完成：%d / %d，失败 %d", task[2], task[3], task[4]))
            if refresh() then savePreferences() end
        elseif task[1] == "cancelled" then
            status("已取消，完成导入的条目已保留")
            refresh(); savePreferences()
        elseif task[1] == "failed" or task[1] == "unavailable" then
            status("导入未完成：" .. tostring(task[5]))
        end
    end
    previousTask = task[1]
    if playing and not self:audio_playing() then moveSong(1) end
end

function shutdown()
    self:audio_stop()
end
