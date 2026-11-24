local pressed, text, imports, imagesSet = {}, {}, {}, {}
local rows = {
    music = {{"a", "First", "media:/music/a", true}, {"b", "Second", "media:/music/b", true}},
    images = {{"p", "Picture", "media:/images/p", true}, {"q", "Second image", "media:/images/q", true}}
}
local stored = {1, "b", "q", false}
local task = {"idle", 0, 0, 0, ""}
local selected, active, starts = "", false, 0
self = {}
function self:pressed(action) return pressed[action] == true end
function self:ui_text(id, value) text[id] = value end
function self:ui_visible() end
function self:try_ui_image(id, reference) imagesSet[id] = reference; return true end
function self:read_save(slot) assert(slot == "local-media-view"); return stored end
function self:try_write_save(slot, value) assert(slot == "local-media-view"); stored = value; return true end
function self:media_entries(kind) return rows[kind] end
function self:try_media_entries(kind) return {true, rows[kind], ""} end
function self:media_status() return task end
function self:media_import(kind, directory) imports[#imports + 1] = {kind, directory}; return true end
function self:media_cancel() task = {"cancelled", 0, 0, 0, ""} end
function self:set(component, field, value)
    assert(component == "azure.audio-source")
    if field == "asset" then selected = value end
end
function self:try_audio_play() starts = starts + 1; active = true; return true end
function self:audio_stop() active = false end
function self:audio_playing() return active end
local function step(action)
    pressed = action and {[action] = true} or {}
    update(.016)
    pressed = {}
end
init(); step()
assert(text["media-music-title"] == "Second" and imagesSet["media-preview"] == "media:/images/q")
step("media-music-next")
assert(stored[2] == "a" and text["media-music-title"] == "First")
step("media-play")
assert(selected == "media:/music/a" and starts == 1 and stored[4] == true)
step("media-play")
assert(not active and stored[4] == false)
step("media-image-next")
assert(stored[3] == "p" and imagesSet["media-preview"] == "media:/images/p")
step("media-import-music"); step("media-folder-images")
assert(imports[1][1] == "music" and imports[1][2] == false)
assert(imports[2][1] == "images" and imports[2][2] == true)
task = {"completed", 3, 3, 1, "broken input"}; step()
assert(text["media-status"]:find("1", 1, true))
stored = {99, "future", "future", true}
step("media-music-next")
assert(stored[1] == 99 and stored[2] == "future", "Newer preference payload must be preserved")
rows.music = {}; rows.images = {}
step("media-refresh"); step("media-play")
assert(starts == 1, "An empty collection must remain actionable without starting audio")
shutdown()
assert(not active)
print("Local media controller selection, playback, partial failure, preferences and empty lists passed")
