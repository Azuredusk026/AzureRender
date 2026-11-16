local rules = {
    longWorkStart = {seconds = 7200, percent = 60},
    continuousWorkStart = {seconds = 300, percent = 90},
    shortWorkedBreakStart = {seconds = 600, percent = 20},
    longWorkedBreakStart = {seconds = 3000, percent = 20},
    longWorkFinish = {seconds = 14400, percent = 60},
    shortWorkFinish = {seconds = 2400, percent = 20},
    midwayFinish = {percent = 50},
}
local probability = .1
local function random(limit) if limit then return limit end return probability end
local initial = {pomodoroRunning = true, round = 1, planned = 7199, intervalWorked = 601, totalWorked = 2401}
local function expect(event, countup, expected, context)
    assert(timer_voice_scenario(rules,event,countup,context or initial,random) == expected)
end

expect("work", false, 4)
initial.planned = 7200
initial.previousWorkEnd, initial.now = 100, 400
expect("work", false, 1001)
expect("work", true, 3001)
probability = .8
expect("work", false, 1000)
initial.now = 400.001
expect("work", false, 4)
initial.now, initial.round = 400, 2
expect("work", false, 4)

probability = .1
expect("break", false, 5)
initial.intervalWorked = 600
expect("break", false, 1100)
expect("break", true, 3101)
initial.intervalWorked = 3000
expect("break", false, 1101)
expect("break", true, 3102)

expect("finish", false, 6)
initial.totalWorked = 14400
expect("finish", false, 1200)
expect("finish", true, 3202)
initial.totalWorked, initial.midway = 2400, true
expect("finish", false, 1201)
expect("finish", true, 3201)
probability = .4
expect("finish", false, 1202)
expect("finish", true, 3200)
probability = .5 + .001
expect("finish", false, 6)

local rows = {
    [4] = {{localization = "shared"}, {localization = "work"}},
    [3000] = {{localization = "shared"}, {localization = "countup"}},
}
local bank = new_timer_voice_bank(rows, random)
assert(bank:take(4).localization == "shared")
assert(bank:take(3000).localization == "countup")
assert(bank:take(4).localization == "work")
assert(bank:take(4).localization == "shared")
