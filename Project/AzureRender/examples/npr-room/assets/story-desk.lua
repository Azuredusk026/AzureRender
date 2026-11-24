function new_story_desk(host, poses)
    local desk, actors = {}, {}
    local motion = {Text = true, ChangeMotion = true, AddTextLoop = true, DirectionGameStartCamera = true}
    for _, pose in pairs(poses) do
        for _, entry in ipairs(pose) do actors[entry.node] = actors[entry.node] or assert(host:find(entry.node)) end
    end

    function desk:begin(chapter)
        for _, row in ipairs(chapter.commands) do
            if motion[row.command] then return end
            local pose = row.command == "ChangeLayout" and poses[row.arguments[1]]
            if pose then
                for _, entry in ipairs(pose) do
                    local actor = actors[entry.node]
                    actor:set("azure.transform", "translation", entry.translation)
                    actor:set("azure.transform", "rotation", entry.rotation)
                end
            end
        end
    end

    return desk
end
