"""Reproduce shutdown with original device settings and bounded failure cleanup."""
import importlib.util
import sys
from pathlib import Path
ROOT=Path.cwd();OUT=ROOT/'build/p8-format-closure-2026-10-12'
sys.path.insert(0,str(ROOT/'tools'))
spec=importlib.util.spec_from_file_location('run_playable_long_run',OUT/'diagnostic-long-run.py')
module=importlib.util.module_from_spec(spec);sys.modules[spec.name]=module;spec.loader.exec_module(module)
from run_platformer_long_run import PlatformerScenario
module.run(OUT/'arena-delivery/Moved exploration',OUT/'platformer-shutdown-probe',90,True,
    scenario=PlatformerScenario())
