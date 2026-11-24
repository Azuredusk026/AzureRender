local layouts = {
    Book_1 = {"one", "soft"}, Book_2 = {"two", "pile"},
    Book_3 = {"three", "pile"}, Book_4 = {"four"}, Book_5 = {"five", "soft"},
}
local saved, visible, attempts, deny = {}, {}, 0, false
local host = {}
function host:find(id)
    return {set = function(_, kind, field, value)
        assert(kind == "azure.renderable" and field == "visible")
        visible[id] = value
    end}
end
function host:read_save(slot) return saved[slot] end
function host:try_write_save(slot, value)
    assert(slot == "room-book-layout")
    attempts = attempts + 1
    if deny then return false end
    saved[slot] = value
    return true
end
local function row(command, name) return {command = command, arguments = {name}} end
local layout = row("ChangeLayout", "Book_1")
local late = row("ChangeLayout", "Book_3")
local main = {category = 1, commands = {row("WaitForSeconds"), layout, row("Text"), late}}
local tutorial = {category = 18, commands = {layout, row("ChangeLayout", "Pc"), row("Text")}}
local function expect(name)
    local members = {}
    for _, node in ipairs(layouts[name]) do members[node] = true end
    for _, nodes in pairs(layouts) do
        for _, node in ipairs(nodes) do assert(visible[node] == (members[node] or false), node) end
    end
end
local books = new_story_book_layout(host, layouts, "Book_1", "later_01", 1)
expect("Book_1")
local prior = {1, "Book_5"}
saved["room-book-layout"] = prior
books:finish()
expect("Book_5")
books:begin(tutorial, false)
expect("Book_1")
books:effect(row("ChangeLayout", "Pc"))
expect("Book_1")
books:finish()
expect("Book_5")
books:begin(main, true)
expect("Book_1")
books:effect(late)
expect("Book_3")
books:finish()
expect("Book_5")
assert(attempts == 0 and saved["room-book-layout"] == prior)

saved["room-unlocks"] = {1, {"tutorial_01", "later_01"}, {}, {}}
books:begin(main, false)
expect("Book_1")
books:finish()
expect("Book_5")
assert(attempts == 0)
saved["room-unlocks"] = {1, {"tutorial_01"}, {}, {}}
deny = true
books:begin(main, false)
expect("Book_1")
books:finish()
expect("Book_5")
assert(attempts == 1 and saved["room-book-layout"] == prior)
deny = false
books:begin(main, false)
books:effect(layout)
books:effect(late)
expect("Book_3")
books:finish()
expect("Book_1")
assert(attempts == 2 and saved["room-book-layout"][2] == "Book_1")

saved["room-book-layout"] = {2, "Book_5"}
books:begin(main, false)
books:finish()
expect("Book_1")
assert(attempts == 2 and saved["room-book-layout"][1] == 2)
