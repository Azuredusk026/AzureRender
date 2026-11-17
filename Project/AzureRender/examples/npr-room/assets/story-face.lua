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

function new_story_face(host, config)
    local actor = assert(host:find(config.node))
    local neutral = assert(config.facial["0"])
    local facial, speak
    local active, ending, published = false, false, ""
    local face = {}

    local function publish(value)
        if published == value then return end
        published = value
        actor:set("azure.animator", "expression", value)
    end

    function face:expression(row)
        if active and row.faceMotion and row.faceMotion >= 0 then
            facial.wanted = assert(config.facial[tostring(row.faceMotion)])
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
            for _, row in ipairs(chapter.commands) do
                if row.command == "Text" or row.command == "ChangeMotion"
                    or row.command == "AddTextLoop" or row.command == "DirectionGameStartCamera" then
                    self:expression(row)
                    break
                end
            end
        end
    end

    function face:finish()
        if not active then return end
        ending = true
        facial.wanted, speak.wanted = neutral, config.silent
    end

    function face:update(dt)
        if not active then return end
        facial:advance(dt)
        speak:advance(dt)
        if ending and not facial.next and facial.current == neutral
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
        publish(config.prefix .. table.concat(morphs, ",") .. ']}' )
    end

    return face
end
