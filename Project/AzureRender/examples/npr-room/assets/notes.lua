local slot = "notes"
local pages
local nextId
local selected
local open
local writable

local function load()
    local saved = self:read_save(slot)
    writable = type(saved) ~= "table" or type(saved[1]) ~= "number" or saved[1] <= 1
    if type(saved) ~= "table" or saved[1] ~= 1 or type(saved[2]) ~= "number"
        or saved[2] % 1 ~= 0 or saved[2] < 2 or type(saved[3]) ~= "table" then
        return {{1, "New Page", ""}}, 2
    end
    local identities = {}
    for _, page in ipairs(saved[3]) do
        if type(page) ~= "table" or type(page[1]) ~= "number" or page[1] % 1 ~= 0
            or page[1] < 1 or page[1] >= saved[2] or identities[page[1]]
            or type(page[2]) ~= "string" or type(page[3]) ~= "string" then
            return {{1, "New Page", ""}}, 2
        end
        identities[page[1]] = true
    end
    if #saved[3] == 0 then
        return {{1, "New Page", ""}}, 2
    end
    return saved[3], saved[2]
end

local function publish(proposed, id)
    if not writable then
        self:ui_text("note-status", "This notebook requires a newer version.")
        return false
    end
    if not self:try_write_save(slot, {1, id, proposed}) then
        self:ui_text("note-status", "Could not save this note.")
        return false
    end
    pages, nextId = proposed, id
    self:ui_text("note-status", "")
    return true
end

local function showPage()
    local page = pages[selected]
    self:ui_visible("note-fields", true)
    self:ui_text("note-page", string.format("%d / %d", selected, #pages))
    self:ui_set_value("note-title", page[2])
    self:ui_set_value("note-text", page[3])
end

local function savePage()
    local current = pages[selected]
    local title, text = self:ui_value("note-title"), self:ui_value("note-text")
    if title == current[2] and text == current[3] then return true end
    local changed = {table.unpack(pages)}
    changed[selected] = {current[1], title, text}
    return publish(changed, nextId)
end

local function addPage()
    if not savePage() then return end
    local changed = {table.unpack(pages)}
    changed[#changed + 1] = {nextId, "New Page", ""}
    if publish(changed, nextId + 1) then
        selected = #pages
        showPage()
    end
end

local function movePage(destination)
    if destination < 1 or destination > #pages or not savePage() then return end
    local changed = {table.unpack(pages)}
    local current = table.remove(changed, selected)
    table.insert(changed, destination, current)
    if publish(changed, nextId) then
        selected = destination
        showPage()
    end
end

local function selectPage(destination)
    if destination < 1 or destination > #pages or not savePage() then return end
    selected = destination
    showPage()
end

local function deletePage()
    if #pages < 2 then return end
    local changed = {table.unpack(pages)}
    table.remove(changed, selected)
    if publish(changed, nextId) then
        selected = math.min(selected, #pages)
        showPage()
    end
end

function init()
    pages, nextId = load()
    selected = 1
    open = false
    self:ui_visible("notes-toggle", true)
    self:ui_visible("notes-panel", false)
end

function update()
    if self:pressed("notes-toggle") then
        if open and not savePage() then return end
        open = not open
        self:ui_visible("notes-panel", open)
        if open then showPage() end
    elseif open then
        if self:pressed("notes-new") then addPage()
        elseif self:pressed("notes-save") then savePage()
        elseif self:pressed("notes-delete") then deletePage()
        elseif self:pressed("notes-prev") then selectPage(selected - 1)
        elseif self:pressed("notes-next") then selectPage(selected + 1)
        elseif self:pressed("notes-up") then movePage(selected - 1)
        elseif self:pressed("notes-down") then movePage(selected + 1)
        end
    end
end
