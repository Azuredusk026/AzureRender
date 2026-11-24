local transforms, writes = {}, 0
local setup = {
    Pc = {{node = "keyboard", translation = {1, 2, 3}, rotation = {0, 40, 0}},
          {node = "textbook", translation = {4, 5, 6}, rotation = {7, 8, 9}}},
    Book = {{node = "keyboard", translation = {4, 5, 6}, rotation = {0, 90, 0}},
            {node = "textbook", translation = {1, 2, 3}, rotation = {0, 20, 0}}},
}
local host = {}
function host:find(node)
    transforms[node] = transforms[node] or {}
    return {set = function(_, component, key, value)
        assert(component == "azure.transform")
        transforms[node][key] = value
        writes = writes + 1
    end}
end
local function layout(name)
    return {command = "ChangeLayout", arguments = {name}}
end
local desk = new_story_desk(host, setup)
desk:begin({commands = {layout("Book_1"), layout("Book"), {command = "Text"}, layout("Pc")}})
assert(writes == 4 and transforms.keyboard.translation[1] == 4 and transforms.textbook.rotation[2] == 20)
desk:begin({commands = {layout("Book_1"), layout("Pc"), {command = "ChangeMotion"}}})
assert(writes == 8 and transforms.keyboard.translation[1] == 1 and transforms.textbook.rotation[3] == 9)
desk:begin({commands = {{command = "Text"}, layout("Book")}})
assert(writes == 8)
