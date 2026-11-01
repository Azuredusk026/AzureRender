import json
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;paths=[root/'build/runs/20261101-120125-character-environment-reception-fa70a7/candidate-project/scene/fullbody-world-aa0.azurelevel',root/'build/runs/20261101-120125-character-environment-reception-fa70a7/candidate-project/scene/fullbody-source-aa0.azurelevel',root/'build/runs/20261031-135416-character-portrait-recovery-0773ca/original-reference/project/high/reference.azurelevel'];rows=[]
for p in paths:
 d=json.loads(p.read_text());settings=d['renderSettings'];rows.append(dict(path=p.relative_to(root).as_posix(),nprLighting=settings.get('nprLighting')))
(run/'scene-npr-source-controls.json').write_bytes((json.dumps(rows,indent=2)+'\n').encode())
for r in rows:print(r['path'],(r['nprLighting'] or {}).get('specularFloor','omitted'))
