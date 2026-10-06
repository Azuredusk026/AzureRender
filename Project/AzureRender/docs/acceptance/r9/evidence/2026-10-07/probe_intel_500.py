"""Focused unchanged-budget probe after eliminating unused GPU deformation."""
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools'))
import run_visibility_scale_benchmark as benchmark
drivers=json.loads((root/'docs/acceptance/p1/evidence/2026-10-05/driver-inventory.json').read_text())
driver=next(row for row in drivers if row['DriverDesc']=='Intel(R) UHD Graphics')
benchmark.COUNTS=(500,)
sys.argv=[str(root/'tools/run_visibility_scale_benchmark.py'), '--output',
    str(root/'build/evolution/r9/deformation-intel-500'), '--driver', driver['VulkanDriverName']]
raise SystemExit(benchmark.main())
