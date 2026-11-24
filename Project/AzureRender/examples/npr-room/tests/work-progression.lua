local parameters = {
    exp = {minutes = 1, perBlock = 10, maxHours = 12},
    points = {minutes = 1, perBlock = 10, maxHours = 12},
    levelThresholds = {1, 250, 350},
    fallbackThreshold = 3000,
}

local function expect(level, exp, points, worked, resultLevel, resultExp, resultPoints)
    local state = settle_work_reward(parameters, {level, exp, points}, worked, 0)
    assert(state[1] == resultLevel and math.abs(state[2] - resultExp) < 1e-8 and state[3] == resultPoints)
end

expect(1, 0, 0, 3, 2, 0, 0)
expect(2, 0, 0, 60, 2, 10, 10)
expect(2, 248, 10, 60, 3, 8, 20)
expect(32, 0, 5, 86400, 32, 0, 7205)
local unrestricted = settle_work_reward(parameters, {32, 0, 5}, 86400, 31)
assert(unrestricted[1] == 34 and unrestricted[2] == 1200 and unrestricted[3] == 7205)
