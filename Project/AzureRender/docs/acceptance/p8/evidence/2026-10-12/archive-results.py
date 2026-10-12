"""Archive P8 observations without copying executable packages."""
import gzip
import hashlib
import json
from pathlib import Path

ROOT=Path.cwd()
SCRATCH=ROOT/'build/p8-format-closure-2026-10-12'
OUT=ROOT/'docs/acceptance/p8/evidence/2026-10-12'
OUT.mkdir(parents=True,exist_ok=True)
selected=[]
for path in SCRATCH.iterdir():
    if path.is_file() and path.suffix in ('.json','.log','.py','.cpp') and path.name!='stage-baseline.json':
        selected.append(path)
for name in ('native-diagnostic','final-formal-performance','standard-long-run','platformer-long-run',
    'standard-long-run-final','platformer-long-run-final','platformer-shutdown-probe'):
    directory=SCRATCH/name
    if directory.exists():
        for path in directory.rglob('*'):
            if path.is_file() and path.suffix in ('.json','.jsonl','.log','.csv') and not any(
                part in ('assets','.azure','config') for part in path.relative_to(directory).parts):
                selected.append(path)
delivery=SCRATCH/'arena-delivery'
for path in delivery.glob('*'):
    if path.is_file() and path.suffix in ('.json','.log'):
        selected.append(path)
for path in (delivery/'packages').glob('*.json'):
    selected.append(path)
for device in ('nvidia','intel'):
    for case in delivery.glob(device+'-*'):
        if case.is_dir():
            for name in ('runtime.json.gz','timing.json','input.json','captures/gpu_capabilities.json'):
                path=case/name
                if path.is_file(): selected.append(path)
files=[]
for source in sorted(set(selected)):
    relative=source.relative_to(SCRATCH)
    raw=source.read_bytes()
    compressed=len(raw)>1024*1024 and source.suffix!='.gz'
    target=OUT/relative
    if compressed: target=target.with_name(target.name+'.gz')
    target.parent.mkdir(parents=True,exist_ok=True)
    stored=gzip.compress(raw,mtime=0) if compressed else raw
    target.write_bytes(stored)
    assert (gzip.decompress(target.read_bytes()) if compressed else target.read_bytes())==raw
    files.append(dict(path=target.relative_to(OUT).as_posix(),sha256=hashlib.sha256(stored).hexdigest(),bytes=len(stored),
        originalSha256=hashlib.sha256(raw).hexdigest(),originalBytes=len(raw),gzip=compressed))
(OUT/'.gitattributes').write_text('** -text -whitespace\n',encoding='utf-8')
(OUT/'manifest.json').write_text(json.dumps(dict(schemaVersion=1,files=files,
    sourceHash=json.loads((SCRATCH/'formal-results.json').read_text(encoding='utf-8'))['sourceHash'],
    regressionPassed=True,deliveryPassed=True,formalPerformancePassed=False,longRunFinalGatesPassed=False,
    physicalInputVerified=False,subjectiveAudioVerified=False,humanEfficiencyVerified=False,
    complete=False,status='AutomaticResultsRecordedFinalGatesPending'),ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('Archived and read back',len(files),'observations')
