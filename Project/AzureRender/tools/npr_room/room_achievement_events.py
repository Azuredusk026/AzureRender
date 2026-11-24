"""Assemble the local achievement listeners in a prepared opening room."""

from tools.npr_room.build_story_signal_room import EXAMPLE


def source(name):
    return (EXAMPLE / name).read_text(encoding="utf-8").rstrip()


def section(program, start, end, replacement):
    if program.count(start) != 1 or program.count(end) != 1:
        raise ValueError(f"Expected one room achievement integration boundary: {start}")
    first, last = program.index(start), program.index(end)
    if last <= first:
        raise ValueError(f"Room achievement integration boundaries are reversed: {start}")
    return program[:first] + replacement.rstrip() + "\n\n" + program[last:]


def bind(timer):
    achievement = source("room-achievements.lua")
    timer = section(timer, 'local workSlot = "work-calendar"', "local room_story_chapters =",
                    source("work-calendar.lua"))
    timer = section(timer, "function new_story_player(", "function new_story_music(", source("story-player.lua"))
    timer_source = source("pomodoro.lua")
    for start, updated, end in (
        ("local roomOpening, timerVoiceActive", "local roomOpening, timerVoiceActive", "local function durationText("),
        ("local function finish(midway)", "local function syncTimerAchievement()", "local function finishInterval("),
        ("function init()", "function init()", "    updateDisplay()\n    if new_room_opening then"),
    ):
        timer = section(timer, start, end, timer_source[timer_source.index(updated):timer_source.index(end)])
    return {name: achievement + "\n\n" + controller + "\n" for name, controller in (
        ("pomodoro.lua", timer), ("notes.lua", source("notes.lua")), ("todo.lua", source("todo.lua")),
    )}
