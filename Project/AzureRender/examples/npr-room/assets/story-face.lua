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

local function story_gaze(faceConfig)
    local config = faceConfig.look
    local value, tween = 0, nil
    local gaze = {}

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
        return value
    end

    function gaze:sample(morphs, upper)
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
        return offsets
    end

    return gaze
end

local function story_plank_sample(config, clock)
    local base = math.floor(clock)
    local fraction = clock - base
    local left = config.frames[base + 1]
    local right = config.frames[(base + 1) % #config.frames + 1]
    local samples = {}
    for index, a in ipairs(left) do
        local b = right[index]
        local values = {}
        for channel = 1, 7 do
            values[channel] = string.format("%.9g", a[channel] + (b[channel] - a[channel]) * fraction)
        end
        samples[index] = ',"rotation":[' .. table.concat(values, ",", 1, 4)
            .. '],"translation":[' .. table.concat(values, ",", 5, 7) .. ']}'
    end
    return samples
end

local function story_expression(host, config)
    local actors, published, clock = {}, {}, 0
    local function actor(node)
        if not actors[node] then actors[node] = {target = assert(host:find(node))} end
        return actors[node]
    end
    local face = actor(config.node)
    face.prefix = config.prefix:sub(1, -#',"morphs":[' - 1)
    for _, track in ipairs(config.look and config.look.targets or {}) do
        actor(track.node).look = track
    end
    for _, track in ipairs(config.plank and config.plank.targets or {}) do
        actor(track.node).plank = track
    end

    return function(dt, morphs, offsets)
        local samples
        if config.plank then
            clock = (clock + dt * config.plank.sampleRate) % #config.plank.frames
            samples = story_plank_sample(config.plank, clock)
        end
        for node, part in pairs(actors) do
            local prefix, sections = part.prefix, {}
            if part.plank then
                prefix = part.plank.prefix
                local bones = {}
                for index, joint in ipairs(part.plank.joints) do
                    bones[index] = joint.prefix .. samples[joint.index]
                end
                sections[#sections + 1] = ',"bones":[' .. table.concat(bones, ",") .. ']'
            end
            if offsets and part.look then
                prefix = part.look.prefix:sub(1, -#',"humanoidRotations":[' - 1)
                local joints = {}
                for index, joint in ipairs(part.look.joints) do
                    joints[index] = joint.prefix .. offsets[joint.index] .. ']}'
                end
                sections[#sections + 1] = ',"humanoidRotations":[' .. table.concat(joints, ",") .. ']'
            end
            if part == face and morphs then
                sections[#sections + 1] = ',"morphs":[' .. table.concat(morphs, ",") .. ']'
            end
            local pose = #sections > 0 and (prefix .. table.concat(sections) .. '}') or ""
            if pose ~= (published[node] or "") then
                part.target:set("azure.animator", "expression", pose)
                published[node] = pose
            end
        end
    end
end

function new_story_face(host, config)
    local neutral = assert(config.facial["0"])
    local gaze = config.look and story_gaze(config)
    local publish = story_expression(host, config)
    local facial, speak
    local active, ending = false, false
    local face = {}

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
        if not active then
            if config.plank then publish(dt) end
            return
        end
        facial:advance(dt)
        speak:advance(dt)
        local look = gaze and gaze:update(dt) or 0
        if ending and look == 0 and not facial.next and facial.current == neutral
            and not speak.next and speak.current == config.silent then
            active = false
            publish(dt)
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
        local offsets = gaze and gaze:sample(morphs, weights)
        publish(dt, morphs, offsets)
    end

    return face
end
