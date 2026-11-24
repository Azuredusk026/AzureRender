"""Incrementally back up inactive generated caches, verify payloads, then retire."""
from pathlib import Path
import hashlib,json,os,shutil,sys,time,zipfile
OUT=Path(__file__).resolve().parent
E=OUT.parents[2]
sys.path.insert(0,str(E/'tools'))
from artifact_cleanup import is_link
from build_provenance import digest
run=E/'build/runs/20261103-234012-character-reference-stages-5abe6c'
active=run/'qualified-lit-surfaces-joint-v4'
inventory=json.loads((OUT/'large-run-inventory.json').read_text())
archive=E/'build/archives/generated-cache-recovery-20261106.zip'
assert not archive.exists(),'Recovery archive already exists; preserve it and resume from its journal'
report=dict(authorization='Current user autonomous cleanup authorization',scope='Inactive generated .azure/cache/*.azcache only; current joint project cache retained',freeBefore=shutil.disk_usage(E).free,archive=str(archive),removed=[],skipped=[])
manifest=OUT/'cache-backup-results.json'
def save():
    report['freeAfter']=shutil.disk_usage(E).free
    manifest.write_text(json.dumps(report,indent=2),encoding='utf-8')
save()
old_archive=E/'build/archives/development-evidence-20261029.tar.zst'
checkpoint=json.loads((OUT/'archive-checkpoint.json').read_text())
st=old_archive.stat()
assert checkpoint['identity']==[st.st_size,st.st_mtime_ns,st.st_ino,st.st_dev]
records=json.loads(Path(str(old_archive)+'.manifest.json').read_text())
assert digest(Path(str(old_archive)+'.manifest.json'))==json.loads(Path(str(old_archive)+'.receipt.json').read_text())['manifestSha256']
known={}
for name,r in records.items():
    if name.endswith('.azcache') and 'sha256' in r:
        known.setdefault(r['sha256'],dict(archive=str(old_archive),member=name))
del records
files=sorted(inventory['caches'],key=lambda x:x['bytes'],reverse=True)
new_members={}
archive.parent.mkdir(exist_ok=True)
started=time.monotonic()
for ordinal,item in enumerate(files):
    if shutil.disk_usage(E).free>=120*2**30: break
    p=Path(item['path'])
    if p.is_relative_to(active):continue
    try:
        assert p.is_relative_to(run) and p.suffix=='.azcache' and '.azure' in p.parts and 'cache' in p.parts
        assert not any(is_link(q) for q in [p,*p.parents] if q.exists())
        st=p.stat()
        assert st.st_size==item['bytes']
        assert shutil.disk_usage(E).free-st.st_size-64*2**20>=5*2**30,'backup reserve'
        before=(st.st_size,st.st_mtime_ns,st.st_ino,st.st_dev)
        sha=digest(p)
        if sha in known:
            location=known[sha]
        else:
            name='sha256/'+sha+'.azcache'
            with zipfile.ZipFile(archive,'a',compression=zipfile.ZIP_DEFLATED,compresslevel=1,allowZip64=True) as z:
                z.write(p,name)
            with zipfile.ZipFile(archive) as z,z.open(name) as source:
                h=hashlib.sha256()
                for chunk in iter(lambda:source.read(4*2**20),b''):h.update(chunk)
                assert h.hexdigest()==sha,'backup payload mismatch'
            location=dict(archive=str(archive),member=name)
            known[sha]=location
            new_members[sha]=location
        after=p.stat()
        assert (after.st_size,after.st_mtime_ns,after.st_ino,after.st_dev)==before,'cache changed'
        row=dict(path=str(p),bytes=before[0],sha256=sha,recovery=location)
        # The journal makes the source-to-archive mapping durable before unlink.
        report['pending']=row
        save()
        p.unlink()
        report['removed'].append(row)
        report.pop('pending')
        if len(report['removed'])%10==0:
            save()
            print(json.dumps(dict(files=len(report['removed']),logicalGiB=sum(r['bytes'] for r in report['removed'])/2**30,freeGiB=report['freeAfter']/2**30,uniqueNewPayloads=len(new_members))),flush=True)
    except (AssertionError,OSError,ValueError) as error:
        report['skipped'].append(dict(path=str(p),reason=str(error)))
        save()
report.update(status='completed',seconds=time.monotonic()-started,logicalBytesRemoved=sum(r['bytes'] for r in report['removed']),newPayloads=len(new_members))
if archive.exists():
    report['archiveSha256']=digest(archive)
    report['archiveBytes']=archive.stat().st_size
save()
print(json.dumps({k:v for k,v in report.items() if k not in ('removed','skipped')}),flush=True)
