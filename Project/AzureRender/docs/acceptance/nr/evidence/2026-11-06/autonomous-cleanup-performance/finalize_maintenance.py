from pathlib import Path
import hashlib,json,shutil,sys,zipfile,subprocess
OUT=Path(__file__).resolve().parent
E=OUT.parents[2]
sys.path.insert(0,str(E/'tools'))
from artifact_budget import check
from build_provenance import digest
report=json.loads((OUT/'cleanup-results.json').read_text())
current=json.loads((OUT/'current-batch.json').read_text())
assert all(not Path(r['path']).exists() for r in current),'Incomplete retirement must be recovered before builds'
assert all(not Path(r['path']).exists() for r in report['removed'])
completed={r['path'] for b in report['batches'] for r in b['items']}
pending=[r for r in report['removed'] if r['path'] not in completed]
if pending:report['batches'].append(dict(items=pending,mainProgramsUnchanged=True,freeAfter=shutil.disk_usage(E).free))
for config in ['release','debug']:
    saved=json.loads((E/'build'/('ninja-msvc-'+config)/'build-provenance.json').read_text())
    player=E/'build'/('ninja-msvc-'+config)/'AzurePlayer.exe'
    assert digest(player)==saved['products'][str(player)]
repo=E.parents[1]
dirty=subprocess.check_output(['git','diff','--binary','--','Project/AzureRender/src','Project/AzureRender/tests','Project/AzureRender/tools/endfield_native','Project/AzureRender/tools/test_native_character_surfaces.py','Project/AzureRender/docs/acceptance/p5'],cwd=repo)
assert hashlib.sha256(dirty).hexdigest()==report['protectedUserDiffSha256']
report.update(status='completed-normal-budget-restored',stopReason='Normal volume reserve passed; unfinished source-snapshot verification retained for later audit',freeAfter=shutil.disk_usage(E).free,protectedUserDiffUnchanged=True,mainProgramsUnchanged=True,logicalBytesRemoved=sum(r['logicalBytes'] for r in report['removed']))
(OUT/'cleanup-results.json').write_text(json.dumps(report,indent=2))
cache=json.loads((OUT/'cache-backup-results.json').read_text())
assert cache['status']=='completed'
assert digest(Path(cache['archive']))==cache['archiveSha256']
sample=next(row for row in cache['removed'] if row['recovery']['archive']==cache['archive'])
restore=OUT/'restore-check.azcache'
with zipfile.ZipFile(cache['archive']) as z,z.open(sample['recovery']['member']) as source,restore.open('wb') as target:
    shutil.copyfileobj(source,target)
assert digest(restore)==sample['sha256']
(OUT/'restore-check.json').write_text(json.dumps(dict(status='passed',source=sample['path'],restored=str(restore),sha256=sample['sha256'],bytes=restore.stat().st_size),indent=2))
budget=check(E/'build',run=OUT,planned=512*2**20)
assert budget['allowed']
(OUT/'budget.json').write_text(json.dumps(budget,indent=2))
manifest_path=E/'docs/development/artifact-manifest.json'
manifest=json.loads(manifest_path.read_text(encoding='utf-8'))
manifest['storageBudget']['deletionApproval']='current-session-authorization'
manifest['recoveryArchives']=manifest.get('recoveryArchives',[])+[dict(path=Path(cache['archive']).relative_to(E/'build').as_posix(),sha256=cache['archiveSha256'],journal=str(OUT.relative_to(E/'build')/'cache-backup-results.json'),scope=cache['scope'])]
manifest_path.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps(dict(expandedRuns=len(report['removed']),cacheFiles=len(cache['removed']),cacheLogicalGiB=cache['logicalBytesRemoved']/2**30,freeGiB=shutil.disk_usage(E).free/2**30,budgetPassed=True,restorePassed=True)),flush=True)
