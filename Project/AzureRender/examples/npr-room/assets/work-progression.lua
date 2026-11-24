local function linearReward(seconds, rule)
    local minutes = math.min(seconds / 60, rule.maxHours * 60)
    return minutes * rule.perBlock / rule.minutes
end

function settle_work_reward(config, previous, seconds, readEpisode)
    local level, exp, points = previous[1], previous[2], previous[3]
    if seconds <= 0 then return {level, exp, points} end

    local function nextThreshold(n)
        return config.levelThresholds[n] or config.fallbackThreshold
    end

    local earned = linearReward(seconds, config.exp)
    if level == 1 then earned = math.max(earned, nextThreshold(1)) end
    if readEpisode < 31 then
        local available = -exp
        for rank = level, 31 do available = available + nextThreshold(rank) end
        earned = math.min(earned, math.max(0, available))
    end
    exp = exp + earned
    while exp >= nextThreshold(level) do
        exp = exp - nextThreshold(level)
        level = level + 1
    end
    return {level, exp, points + math.floor(linearReward(seconds, config.points))}
end
