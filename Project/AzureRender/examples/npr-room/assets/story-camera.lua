function new_story_camera(host, track)
    local actor = assert(host:find(track.node))
    local component = "azure.transform"
    local cue = track.cueNode and assert(host:find(track.cueNode))
    local elapsed
    local sampleCount = #track.rotations
    local camera = {}

    function camera:effect(row)
        if row.command ~= "DirectionGameStartCamera" then return end
        elapsed = 0
        actor:set(component, "translation", track.translation)
        actor:set(component, "rotation", track.rotations[1])
        if cue then cue:audio_play() end
    end

    function camera:update(dt)
        if elapsed == nil or elapsed >= sampleCount - 1 then return end
        elapsed = math.min(elapsed + dt * track.sampleRate, sampleCount - 1)
        local index = math.floor(elapsed + 1e-6) + 1
        local fraction = elapsed - (index - 1)
        local from = track.rotations[index]
        if fraction <= 1e-6 or index == sampleCount then
            actor:set(component, "rotation", from)
            return
        end
        local to = track.rotations[index + 1]
        local rotation = {}
        for axis = 1, 3 do
            local delta = (to[axis] - from[axis] + 180) % 360 - 180
            rotation[axis] = from[axis] + delta * fraction
        end
        actor:set(component, "rotation", rotation)
    end

    function camera:finish()
        if elapsed == nil then return end
        elapsed = nil
        if cue then cue:audio_stop() end
        actor:set(component, "rotation", track.finish)
    end

    return camera
end
