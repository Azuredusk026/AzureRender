"""Run music preference recovery against the production Lua controller."""
import argparse
from pathlib import Path

from test_pomodoro_runtime import run


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lua", type=Path, required=True)
    args = parser.parse_args()
    run(args.lua, [Path("examples/npr-room/tests/music-preferences.lua")])
