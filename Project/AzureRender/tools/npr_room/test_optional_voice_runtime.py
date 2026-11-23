"""Run optional voice failure timelines against production Lua modules."""
import argparse
import os
from pathlib import Path
from test_pomodoro_runtime import run

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lua", type=Path, required=True)
    parser.add_argument("--case", choices=("all", "room", "choice", "timer", "checkpoint"), default="all")
    args = parser.parse_args()
    os.environ["AZURE_VOICE_CASE"] = args.case
    run(args.lua, [Path("examples/npr-room/tests/optional-voice-recovery.lua")])
