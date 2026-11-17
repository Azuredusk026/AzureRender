local function sample_story_face_curve(curve, time)
    if type(curve) == "number" then return curve end
    local times, values, incoming, outgoing, steps = table.unpack(curve)
    local count = #times
    if time <= times[1] then return values[1] end
    if time >= times[count] then return values[count] end
    local left, right = 1, count
    while right - left > 1 do
        local middle = math.floor((left + right) / 2)
        if times[middle] <= time then left = middle else right = middle end
    end
    if steps[left] then return values[left] end
    local duration = times[right] - times[left]
    local t = (time - times[left]) / duration
    local t2, t3 = t * t, t * t * t
    return (2*t3 - 3*t2 + 1) * values[left] + (t3 - 2*t2 + t) * outgoing[left] * duration
        + (3*t2 - 2*t3) * values[right] + (t3 - t2) * incoming[right] * duration
end

local function story_face_layer(initial, durationFor)
    local layer = {current = initial, next = nil, wanted = initial, time = 0, fade = 0}
    function layer:advance(dt)
        self.time = self.time + dt
        if self.next then
            self.fade = self.fade + dt
            if self.fade + 1e-7 >= self.duration then
                self.current, self.time = self.next, self.fade
                self.next, self.fade = nil, 0
            end
        elseif self.wanted ~= self.current then
            self.next, self.fade, self.duration = self.wanted, 0, durationFor(self.wanted)
        end
    end
    return layer
end

local function story_face_sample(track, time)
    if track.loop and track.seconds > 0 then time = time % track.seconds end
    local weights = {}
    for _, entry in ipairs(track.curves) do
        weights[entry[1]] = sample_story_face_curve(entry[2], time)
    end
    return weights
end

local function apply_story_face_layer(layer, defaults, combined)
    local weights = story_face_sample(layer.current, layer.time)
    if layer.next and layer.fade > 1e-7 then
        local nextWeights = story_face_sample(layer.next, layer.fade)
        local fraction = layer.fade / layer.duration
        for index, value in pairs(weights) do
            weights[index] = value * (1 - fraction) + (nextWeights[index] or defaults[index]) * fraction
        end
        for index, value in pairs(nextWeights) do
            if weights[index] == nil then
                weights[index] = defaults[index] * (1 - fraction) + value * fraction
            end
        end
    end
    for index, value in pairs(weights) do combined[index] = value end
end

local function story_gaze(host, faceConfig)
    local config = faceConfig.look
    local value, tween = 0, nil
    local actors, published = {}, {}
    local gaze = {}
    for _, track in ipairs(config.targets) do
        actors[track.node] = assert(host:find(track.node))
    end

    function gaze:select(target, seconds)
        if seconds <= 0 or target == value then
            value, tween = target, nil
            return
        end
        tween = {from = value, to = target, seconds = seconds, elapsed = 0}
    end

    function gaze:update(dt)
        if tween then
            tween.elapsed = math.min(tween.seconds, tween.elapsed + dt)
            local t = tween.elapsed / tween.seconds
            value = tween.to + (tween.from - tween.to) * (1 - t)^2
            if tween.elapsed + 1e-7 >= tween.seconds then value, tween = tween.to, nil end
        end
        if value == 0 then
            for node, actor in pairs(actors) do
                if node ~= faceConfig.node and published[node] then
                    actor:set("azure.animator", "expression", "")
                    published[node] = nil
                end
            end
        end
        return value
    end

    function gaze:pose(morphs, upper)
        if value == 0 then return nil end
        local time = value * config.seconds
        local offsets = {}
        for index, curves in ipairs(config.jointCurves) do
            local numbers = {}
            for axis, curve in ipairs(curves) do
                numbers[axis] = string.format("%.9g", sample_story_face_curve(curve, time))
            end
            offsets[index] = table.concat(numbers, ",")
        end
        for _, entry in ipairs(config.eyes) do
            local index = entry[1]
            if upper[index] == nil then
                local delta = sample_story_face_curve(entry[2], time)
                if delta ~= 0 then
                    morphs[#morphs + 1] = '{"nodePath":' .. faceConfig.nodePath .. ',"targetId":'
                        .. faceConfig.keys[index] .. ',"weight":' .. string.format("%.9g", delta) .. ',"additive":true}'
                end
            end
        end
        local faceResult
        for _, track in ipairs(config.targets) do
            local parts = {}
            for index, joint in ipairs(track.joints) do
                parts[index] = joint.prefix .. offsets[joint.index] .. ']}'
            end
            local pose = track.prefix .. table.concat(parts, ",") .. ']'
            if track.node == faceConfig.node then
                faceResult = pose .. ',"morphs":[' .. table.concat(morphs, ",") .. ']}'
            else
                pose = pose .. '}'
                if published[track.node] ~= pose then
                    actors[track.node]:set("azure.animator", "expression", pose)
                    published[track.node] = pose
                end
            end
        end
        return assert(faceResult)
    end

    return gaze
end

function new_story_face(host, config)
    local actor = assert(host:find(config.node))
    local neutral = assert(config.facial["0"])
    local gaze = config.look and story_gaze(host, config)
    local facial, speak
    local active, ending, published = false, false, ""
    local face = {}

    local function publish(value)
        if published == value then return end
        published = value
        actor:set("azure.animator", "expression", value)
    end

    function face:expression(row)
        if not active then return end
        if row.faceMotion and row.faceMotion >= 0 then
            facial.wanted = assert(config.facial[tostring(row.faceMotion)])
        end
        if gaze and row.look then
            assert(row.look.ease == 0 and row.look.scale >= 0 and row.look.scale <= 1 and row.look.seconds >= 0)
            gaze:select(row.look.scale, row.look.seconds)
        end
    end

    function face:voice(playing)
        if active then speak.wanted = playing and config.voiced or config.silent end
    end

    function face:begin(chapter, replay)
        if not active then
            facial = story_face_layer(neutral, function() return config.faceBlend end)
            speak = story_face_layer(config.silent, function(track)
                return track == config.voiced and config.speakIn or config.speakOut
            end)
        end
        active, ending = true, false
        if replay or chapter.category == 1 then
            if gaze then gaze:select(0, 0) end
            for _, row in ipairs(chapter.commands) do
                if row.command == "Text" or row.command == "ChangeMotion"
                    or row.command == "AddTextLoop" or row.command == "DirectionGameStartCamera" then
                    self:expression({faceMotion = row.faceMotion})
                    break
                end
            end
        end
    end

    function face:finish(chapter, replay)
        if not active then return end
        ending = true
        facial.wanted, speak.wanted = neutral, config.silent
        if gaze then gaze:select(0, chapter and chapter.category == 18 and not replay and 1.5 or 0) end
    end

    function face:update(dt)
        if not active then return end
        facial:advance(dt)
        speak:advance(dt)
        local look = gaze and gaze:update(dt) or 0
        if ending and look == 0 and not facial.next and facial.current == neutral
            and not speak.next and speak.current == config.silent then
            active = false
            publish("")
            return
        end
        local weights = {}
        apply_story_face_layer(facial, config.defaults, weights)
        apply_story_face_layer(speak, config.defaults, weights)
        local morphs = {}
        for index, key in ipairs(config.keys) do
            if weights[index] ~= nil then
                morphs[#morphs + 1] = '{"nodePath":' .. config.nodePath .. ',"targetId":' .. key
                    .. ',"weight":' .. string.format("%.9g", weights[index]) .. '}'
            end
        end
        local pose = gaze and gaze:pose(morphs, weights)
        publish(pose or (config.prefix .. table.concat(morphs, ",") .. ']}'))
    end

    return face
end
