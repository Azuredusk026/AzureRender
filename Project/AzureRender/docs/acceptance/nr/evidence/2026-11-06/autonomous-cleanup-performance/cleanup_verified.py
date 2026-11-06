"""Session-authorized retirement of byte-verified expanded archive copies."""
from pathlib import Path
import hashlib,json,os,shutil,subprocess,sys,time
E=Path(__file__).resolve().parents[3]
OUT=Path(__file__).resolve().parent
sys.path.insert(0,str(E/'tools'))
from artifact_cleanup import is_link,remove_artifact
from build_provenance import digest
archive=E/'build/archives/development-evidence-20261029.tar.zst'
receipt=json.loads(Path(str(archive)+'.receipt.json').read_text())
assert receipt['status']=='verified'
report=dict(authorization='2026-11-06 user grants autonomous directory cleanup without further confirmation',freeBefore=shutil.disk_usage(E).free,batches=[],skipped=[],removed=[],protected=[])
if (OUT/'cleanup-results.json').exists():
    report=json.loads((OUT/'cleanup-results.json').read_text())
    report['resumeCount']=report.get('resumeCount',0)+1
    report['resumeReason']='Restrict tracked-file protection inventory to build subtree; all retirements retain archive verification'
def save():
    report['freeAfter']=shutil.disk_usage(E).free
    (OUT/'cleanup-results.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
save()
print('Checking archive and member-index SHA256',flush=True)
checkpoint=OUT/'archive-checkpoint.json'
state=archive.stat()
identity=[state.st_size,state.st_mtime_ns,state.st_ino,state.st_dev]
previous=json.loads(checkpoint.read_text()) if checkpoint.exists() else {}
if previous.get('identity')!=identity or previous.get('sha256')!=receipt['archiveSha256']:
    assert digest(archive)==receipt['archiveSha256']
    checkpoint.write_text(json.dumps(dict(identity=identity,sha256=receipt['archiveSha256'],verified=True),indent=2))
assert digest(Path(str(archive)+'.manifest.json'))==receipt['manifestSha256']
report['archiveSha256']=receipt['archiveSha256']
archive_state=archive.stat()
records=json.loads(Path(str(archive)+'.manifest.json').read_text())
def expected(name):
    seen=set()
    while 'link' in records[name]:
        assert name not in seen
        seen.add(name)
        name=records[name]['link']
    return records[name]['sha256']
frozen=json.loads((E/'build/runs/20261105-125529-automatic-acceptance-3d5b54/prepared-resource-paths/image/frozen-inputs.json').read_text())['project']
protected={Path(p).relative_to(E/'build/runs').parts[0] for p in frozen if Path(p).is_relative_to(E/'build/runs')}
protected.update(['20261103-234012-character-reference-stages-5abe6c','20261105-125529-automatic-acceptance-3d5b54',OUT.name])
report['protected']=sorted(protected)
repo=E.parents[1]
tracked_names=subprocess.check_output(['git','ls-files','--full-name','-z'],cwd=repo).decode('utf-8').split('\0')
tracked={repo/p for p in tracked_names if p and (repo/p).is_relative_to(E/'build')}
dirty=subprocess.check_output(['git','diff','--binary','--', 'Project/AzureRender/src','Project/AzureRender/tests','Project/AzureRender/tools/endfield_native','Project/AzureRender/tools/test_native_character_surfaces.py','Project/AzureRender/docs/acceptance/p5'],cwd=repo)
report['protectedUserDiffSha256']=hashlib.sha256(dirty).hexdigest()
programs={str(E/'build'/('ninja-msvc-'+c)/'AzurePlayer.exe'):digest(E/'build'/('ninja-msvc-'+c)/'AzurePlayer.exe') for c in ['release','debug']}
manifest_path=E/'docs/development/artifact-manifest.json'
manifest=json.loads(manifest_path.read_text(encoding='utf-8'))
totals={}
for name,r in records.items():
    parts=name.split('/')
    if len(parts)>3 and parts[:2]==['build','runs']:
        totals[parts[2]]=totals.get(parts[2],0)+r['size']
preview=json.loads((E/'docs/acceptance/nr/evidence/2026-11-06/resume-storage-performance/cleanup-preview.json').read_text())
p0=[Path(r['path']).name for r in preview['items'] if 'error' not in r and Path(r['path']).name[:8]<'20261029']
names=list(dict.fromkeys(p0+sorted(totals,key=totals.get,reverse=True)))
names=[n for n in names if n not in protected and n[:8]<'20261029' and (E/'build/runs'/n).is_dir()]
print('Archive verified; candidates',len(names),'protected',len(protected),flush=True)
hash_cache={}
completed={r['path'] for b in report['batches'] for r in b['items']}
batch=[r for r in report['removed'] if r['path'] not in completed]
for name in names:
    if shutil.disk_usage(E).free>=160*2**30: break
    run=E/'build/runs'/name
    assert run.parent.resolve()==(E/'build/runs').resolve() and not is_link(run)
    try:
        assert not any(p==run or run in p.parents for p in tracked),'tracked content'
        states={}
        for base,dirs,files in os.walk(run,followlinks=False):
            for child in [*(Path(base)/d for d in dirs),*(Path(base)/f for f in files)]:
                assert not is_link(child),f'link {child}'
            for filename in files:
                path=Path(base)/filename
                key=path.relative_to(E).as_posix()
                assert key in records,f'not archived: {key}'
                st=path.stat()
                assert st.st_size==records[key]['size'],f'size changed: {key}'
                ident=(st.st_dev,st.st_ino,st.st_size,st.st_mtime_ns)
                if ident not in hash_cache: hash_cache[ident]=digest(path)
                assert hash_cache[ident]==expected(key),f'bytes changed: {key}'
                states[key]=ident
        # Recheck all metadata after hashing, before the irreversible operation.
        for key,ident in states.items():
            st=(E/key).stat()
            assert (st.st_dev,st.st_ino,st.st_size,st.st_mtime_ns)==ident,f'concurrent write: {key}'
        observation=subprocess.run(['powershell','-NoProfile','-Command',"Get-Process AzurePlayer,AzureRender,ctest,cl,link,ninja -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Id"],text=True,capture_output=True)
        assert observation.returncode in (0,1) and not observation.stderr,'process inventory failed'
        active=observation.stdout.strip()
        assert not active,'active build/test processes'
        assert archive.stat().st_mtime_ns==archive_state.st_mtime_ns and archive.stat().st_size==archive_state.st_size
        row=dict(path=str(run),files=len(states),verifiedArchiveMembers=True,freeBefore=shutil.disk_usage(E).free)
        batch.append(row)
        (OUT/'current-batch.json').write_text(json.dumps(batch,indent=2))
        print('Removing archive-verified expanded copy:',str(run),flush=True)
        size,_=remove_artifact(E/'build',run,tracked,apply=True)
        row.update(logicalBytes=size,freeAfter=shutil.disk_usage(E).free)
        report['removed'].append(row)
        old=[r for r in manifest['preservedRuns'] if r['path']=='runs/'+name]
        manifest['preservedRuns']=[r for r in manifest['preservedRuns'] if r['path']!='runs/'+name]
        manifest.setdefault('archivedRuns',[]).append(dict(path='runs/'+name,archive=archive.relative_to(E/'build').as_posix(),archiveSha256=receipt['archiveSha256'],membersVerified=len(states),previousRegistration=old))
        manifest_path.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        save()
        if len(batch)==10:
            assert all(digest(Path(p))==h for p,h in programs.items())
            report['batches'].append(dict(items=batch,mainProgramsUnchanged=True,freeAfter=shutil.disk_usage(E).free))
            batch=[]
            save()
    except (AssertionError,OSError,ValueError) as error:
        report['skipped'].append(dict(path=str(run),reason=str(error)))
        save()
        print('Preserved:',name,str(error),flush=True)
if batch:
    assert all(digest(Path(p))==h for p,h in programs.items())
    report['batches'].append(dict(items=batch,mainProgramsUnchanged=True,freeAfter=shutil.disk_usage(E).free))
assert hashlib.sha256(subprocess.check_output(['git','diff','--binary','--','Project/AzureRender/src','Project/AzureRender/tests','Project/AzureRender/tools/endfield_native','Project/AzureRender/tools/test_native_character_surfaces.py','Project/AzureRender/docs/acceptance/p5'],cwd=repo)).hexdigest()==report['protectedUserDiffSha256']
report['protectedUserDiffUnchanged']=True
report['logicalBytesRemoved']=sum(r['logicalBytes'] for r in report['removed'])
report['status']='completed'
save()
print(json.dumps(dict(removed=len(report['removed']),logicalGiB=report['logicalBytesRemoved']/2**30,freeGiB=report['freeAfter']/2**30,skipped=len(report['skipped']))),flush=True)
