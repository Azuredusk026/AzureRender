from pathlib import Path
import hashlib
import json
import shutil
import sys
root=Path(__file__).resolve().parents[3]
out=Path(__file__).resolve().parent
sys.path.insert(0,str(root/'tools'))
from build_provenance import inputs,digest
recorded=json.loads((root/'build/ninja-msvc-release/build-provenance.json').read_text(encoding='utf-8'))
current=inputs(root)
changed=sorted(p for p in current.keys()|recorded['source'].keys() if current.get(p)!=recorded['source'].get(p))
production=[p for p in changed if not p.startswith('tools/')]
result=dict(sourceBase='23015e7',changedInputs=changed,changedProductionInputs=production,
            releasePlayerSha256=digest(root/'build/ninja-msvc-release/AzurePlayer.exe'),
            expectedPlayerSha256='83304ac0a57077d46d45c9a8227093658af2d9aefcbe0564194e157733d46019',
            provenanceVerification='failed-source-inputs-differ',runtimeRebuilt=False)
(out/'source-check.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
evidence=root/'docs/acceptance/nr/evidence/2026-11-06/resume-storage-performance'
evidence.mkdir(parents=True,exist_ok=True)
for name in ('pin-audit.json','environment.json','source-check.json','resume_audit.py','prepare_receipts.py'):
    shutil.copy2(out/name,evidence/name)
print(json.dumps(result))
