"""Run the original local-media controller against deterministic service results."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent / "npr_room"))
from test_pomodoro_runtime import run

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lua", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1] / "examples/local-media"
    run(args.lua, [root / "assets/media.lua", root / "tests/controller.lua"])
