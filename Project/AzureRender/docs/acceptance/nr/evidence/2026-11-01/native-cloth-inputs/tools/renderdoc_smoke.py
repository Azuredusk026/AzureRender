# CLI source: https://github.com/baldurk/renderdoc/blob/v1.46/qrenderdoc/Code/qrenderdoc.cpp
import json,sys,traceback
from pathlib import Path
run=Path('D:/Projects/FYP/Project/AzureRender/build/runs/20261101-143507-native-cloth-variants-d877ef');report=dict(status='running',python=sys.version)
try:
 import renderdoc as rd
 report.update(status='passed',renderdoc=rd.GetVersionString())
except Exception:report.update(status='failed',error=traceback.format_exc())
(run/'renderdoc-smoke.json').write_bytes((json.dumps(report,indent=2)+'\n').encode());sys.exit(0)
