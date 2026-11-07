"""Indexed, bounded lifecycle for generated task directories."""
import argparse
from contextlib import contextmanager
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import sqlite3
import subprocess
import time
import zipfile

from artifact_cleanup import is_link, remove_artifact

GIB = 2**30
POLICY = dict(poolBytes=16*GIB, coldBytes=8*GIB, runBytes=2*GIB,
              maximumFiles=10000, retentionDays=1, maximumBatch=10,
              maintenanceSeconds=120,
              recoveryDays=7,evidenceDays=30,evidenceBytes=8*2**20,evidencePoolBytes=2*GIB,
              coldRoot=str(Path(__file__).resolve().parents[1]/'build/archives/managed'))


def digest(path):
    value = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024*1024), b''):
            value.update(block)
    return value.hexdigest()


def real_path(path):
    path = Path(path).absolute()
    if '..' in path.parts:
        raise ValueError('Parent traversal is outside the artifact path contract')
    for item in (path, *path.parents):
        if (item.exists() or item.is_symlink()) and is_link(item):
            raise ValueError('Link/reparse point: '+str(item))
    return path.resolve()


def run_path(build, name):
    import re
    if not re.fullmatch(r'[a-z0-9][a-z0-9-]*', name):
        raise ValueError('Expected a direct task directory name')
    return real_path(real_path(build)/'runs'/name)


def metadata(build):
    return real_path(real_path(build)/'maintenance')


def write_json(path, value):
    path = real_path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = real_path(path.with_name(path.name+'.tmp-'+str(os.getpid())))
    temporary.write_text(json.dumps(value, indent=2, ensure_ascii=False)+'\n', encoding='utf-8')
    os.replace(temporary, path)


@contextmanager
def database(build, write=False):
    path = real_path(metadata(build)/'artifact-index.sqlite3')
    if not write and not path.exists():
        yield None
        return
    if write:
        path.parent.mkdir(parents=True, exist_ok=True)
    db = sqlite3.connect(str(path) if write else path.as_uri()+'?mode=ro', uri=not write, timeout=2)
    try:
        if write:
            db.execute('CREATE TABLE IF NOT EXISTS runs(name TEXT PRIMARY KEY, data TEXT NOT NULL)')
        yield db
        if write:
            db.commit()
    finally:
        db.close()


def rows(db):
    return {name: json.loads(data) for name, data in db.execute('SELECT name,data FROM runs')} if db else {}


def save(db, name, value):
    db.execute('INSERT OR REPLACE INTO runs VALUES (?,?)', (name, json.dumps(value)))


def reservation(records):
    return sum(r['reservedBytes'] for r in records.values() if r['status'] != 'released')


def pool_budget(build, *, run=None, used=0, planned=0):
    with database(build) as db:
        records = rows(db)
    current = records.get(Path(run).name) if run else None
    total = reservation(records)
    increment = max(0, used+planned-current['reservedBytes']) if current else (planned if run is None else 0)
    reasons = []
    if current and current['status'] != 'active':
        reasons.append('managed-run-closed-use-resume')
    if total+increment > POLICY['poolBytes']:
        reasons.append('managed-pool-limit')
    return dict(allowed=not reasons, reasons=reasons, reservedBytes=total,
                projectedBytes=total+increment, limitBytes=POLICY['poolBytes'])


def reserve_write(build, run, used, planned):
    result = pool_budget(build, run=run, used=used, planned=planned)
    if not result['allowed']:
        return result
    with database(build) as db:
        if Path(run).name not in rows(db):
            return result
    with database(build, True) as db:
        db.execute('BEGIN IMMEDIATE')
        records = rows(db);record = records[Path(run).name]
        total = reservation(records)
        projected = total+max(0, used+planned-record['reservedBytes'])
        if record['status'] != 'active' or projected > POLICY['poolBytes']:
            return dict(allowed=False, reasons=['managed-pool-or-state-changed'], reservedBytes=total,
                        projectedBytes=projected, limitBytes=POLICY['poolBytes'])
        record['reservedBytes'] = max(record['reservedBytes'], used+planned)
        save(db, Path(run).name, record)
    return dict(allowed=True, reasons=[], reservedBytes=projected, projectedBytes=projected, limitBytes=POLICY['poolBytes'])


def create_run(build, path, budget, inputs=(), *, retention_days=1, retain_recovery=False):
    path = run_path(build, Path(path).name)
    if path.exists():
        raise ValueError('Task already exists')
    if retention_days not in (1,3,7):raise ValueError('Retention must be 1, 3 or 7 days')
    dependencies = [run_path(build, name).name for name in inputs]
    if any(not run_path(build, name).is_dir() for name in dependencies):
        raise ValueError('Input task is absent; restore it first')
    if type(budget['plannedBytes']) is not int or not 0 <= budget['plannedBytes'] <= POLICY['runBytes']:
        raise ValueError('Invalid task reservation')
    planned = max(budget['plannedBytes'], 64*2**20)
    with database(build, True) as db:
        db.execute('BEGIN IMMEDIATE')
        if any(not run_path(build, name).is_dir() for name in dependencies):
            raise ValueError('Input task disappeared before reservation')
        if reservation(rows(db))+planned > POLICY['poolBytes']:
            raise ValueError('Managed task pool exceeds 16 GiB; close and clean completed work')
        path.mkdir()
        write_json(path/'budget.json', dict(budget, managedIndexVersion=1))
        save(db, path.name, dict(status='active', created=time.time(), reservedBytes=planned,
                                inputs=dependencies, archive=None, blocked=None, generation=1,
                                retentionDays=retention_days,retainRecovery=bool(retain_recovery)))


def inventory(path):
    path = real_path(path)
    files, directories, total = {}, [], 0
    for current, dirs, names in os.walk(path, followlinks=False):
        for name in dirs:
            child = real_path(Path(current)/name)
            if name == '.git':
                raise ValueError('Nested Git worktree is protected')
            directories.append(child.relative_to(path).as_posix())
        for name in names:
            if name == '.finished' and Path(current) == path:
                continue
            child = real_path(Path(current)/name)
            if name == '.git':
                raise ValueError('Nested Git worktree is protected')
            stat = child.stat()
            total += stat.st_size
            if total > POLICY['runBytes'] or len(files) >= POLICY['maximumFiles']:
                raise ValueError('Task exceeds byte/file-count budget; preserve for separate maintenance')
            sha = digest(child)
            after = child.stat()
            if (stat.st_size, stat.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
                raise ValueError('Task changed while sealing')
            files[child.relative_to(path).as_posix()] = dict(bytes=stat.st_size, mtimeNs=stat.st_mtime_ns, sha256=sha)
    return dict(files=files, directories=sorted(directories), bytes=total)


def seal(build, path):
    path = run_path(build, Path(path).name)
    if not path.is_dir():
        raise ValueError('Task directory is absent')
    with database(build) as db:
        record = rows(db).get(path.name)
    if not record:
        return False  # Historical task ownership remains with the existing policy.
    if record.get('processPid'):
        raise ValueError('Owned process is still running')
    generation = record.get('generation',0)
    value = inventory(path)
    fingerprint = hashlib.sha256(json.dumps(value,sort_keys=True).encode()).hexdigest()
    manifest = metadata(build)/'sealed'/(path.name+'-'+fingerprint[:16]+'.json')
    write_json(manifest, value)
    evidence = compact_evidence(build,path,value)
    record.update(status='closed', closed=time.time(), reservedBytes=value['bytes'],
                  manifest=str(manifest), manifestSha256=digest(manifest), blocked=None, generation=generation+1,
                  evidence=str(evidence),evidenceSha256=digest(evidence))
    with database(build, True) as db:
        db.execute('BEGIN IMMEDIATE')
        other = rows(db)
        if other[path.name].get('generation',0) != generation:
            raise ValueError('Task lifecycle changed while sealing')
        if not evidence.is_file() or digest(evidence)!=record['evidenceSha256']:
            raise ValueError('Evidence changed before its lifecycle commit')
        if reservation(other)-other[path.name]['reservedBytes']+value['bytes'] > POLICY['poolBytes']:
            raise ValueError('Managed pool exceeded during task output')
        save(db, path.name, record)
    return True


def resume(build, name):
    path = run_path(build, name)
    with database(build, True) as db:
        db.execute('BEGIN IMMEDIATE')
        record = rows(db).get(name)
        if not record or record['status'] == 'released' or not path.is_dir():
            raise ValueError('Task is absent; use restore')
        if record['status'] in {'restoring','blocked-restore'}:
            raise ValueError('Incomplete restoration is protected; inspect its receipt')
        if record.get('processPid'):
            raise ValueError('Owned process is still running')
        record.update(status='active', blocked=None, generation=record.get('generation',0)+1)
        save(db, name, record)
    marker = path/'.finished'
    if marker.exists():
        real_path(marker).unlink()


def expected_inventory(record):
    path = real_path(record['manifest'])
    if digest(path) != record['manifestSha256']:
        raise ValueError('Sealed manifest changed')
    return json.loads(path.read_text(encoding='utf-8'))


def relative_member(name):
    path = PurePosixPath(name)
    if not name or '\\' in name or ':' in name or path.is_absolute() or '..' in path.parts or str(path) != name or '.git' in path.parts:
        raise ValueError('Unsafe archive member: '+name)
    return path


def verify_archive(archive, value, sha=None):
    archive = real_path(archive)
    actual_sha = digest(archive)
    if sha and actual_sha != sha:
        raise ValueError('Recovery archive digest changed')
    with zipfile.ZipFile(archive) as package:
        names = package.namelist()
        wanted = {'inventory.json', *('files/'+name for name in value['files'])}
        if len(names) != len(wanted) or set(names) != wanted:
            raise ValueError('Recovery member inventory differs')
        if json.loads(package.read('inventory.json')) != value:
            raise ValueError('Recovery manifest differs')
        for name, item in value['files'].items():
            relative_member(name)
            info = package.getinfo('files/'+name)
            if info.file_size != item['bytes']:
                raise ValueError('Recovery size differs')
            with package.open(info) as stream:
                h = hashlib.sha256()
                for block in iter(lambda: stream.read(1024*1024), b''):
                    h.update(block)
            if h.hexdigest() != item['sha256']:
                raise ValueError('Recovery payload differs')
    return actual_sha


def cold_usage(cold=None):
    cold = real_path(cold or POLICY['coldRoot'])
    root = cold
    if not root.exists():return 0
    total=0
    for item in root.iterdir():
        item=real_path(item)
        if item.is_file():total+=item.stat().st_size
        elif item.is_dir():
            for child in item.iterdir():
                child=real_path(child)
                if child.is_file():total+=child.stat().st_size
                elif child.is_dir():raise ValueError('Cold store contains an unindexed nested directory')
    return total


def compact_evidence(build,path,value):
    """Keep bounded source/receipts/log excerpts, rather than a second full run."""
    contents={'.artifact-inventory.json':json.dumps(value,ensure_ascii=False).encode('utf-8')}
    source_extensions={'.py','.cpp','.hpp','.h','.c','.patch','.diff','.glsl','.comp','.frag','.vert','.hlsl',
                       '.inc','.inl','.hlsli','.slang','.wgsl','.metal','.cu','.cuh','.cs','.js','.ts','.tsx','.jsx',
                       '.lua','.ps1','.bat','.cmd','.sh','.cmake','.toml','.yaml','.yml','.azureproject','.azscene','.azmaterial'}
    candidates=[]
    for name,item in value['files'].items():
        suffix=Path(name).suffix.lower()
        priority=0 if suffix in source_extensions else 1 if suffix in {'.json','.csv','.txt'} else 2 if suffix=='.log' else 3 if suffix=='.png' else 9
        if priority<9:candidates.append((priority,name,item))
    images=sorted(n for p,n,i in candidates if p==3)
    image_selection={images[0],images[len(images)//2],images[-1]} if images else set()
    total=len(contents['.artifact-inventory.json'])
    for priority,name,item in sorted(candidates):
        if name=='.artifact-inventory.json':raise ValueError('Reserved evidence member name')
        file=path/name
        if priority==2:
            with file.open('rb') as stream:
                data=stream.read(32768)
                if item['bytes']>65536:
                    stream.seek(-32768,2);data+=b'\n--- retained log head/tail ---\n'+stream.read(32768)
                else:data+=stream.read()
        else:
            limit=2*2**20
            if priority==3 and name not in image_selection:continue
            if item['bytes']>limit:
                if priority==0:raise ValueError('Source evidence exceeds compact limit; preserve it in the source project')
                continue
            data=file.read_bytes()
            if hashlib.sha256(data).hexdigest()!=item['sha256']:raise ValueError('Evidence source changed after inventory')
        stat=file.stat()
        if (stat.st_size,stat.st_mtime_ns)!=(item['bytes'],item['mtimeNs']):raise ValueError('Evidence input changed while reading')
        if total+len(data)>POLICY['evidenceBytes']:
            if priority==0:raise ValueError('Source evidence exceeds compact evidence budget')
            continue
        contents[name]=data;total+=len(data)
    if total>POLICY['evidenceBytes']:raise ValueError('Evidence inventory exceeds compact budget')
    fingerprint=hashlib.sha256(json.dumps(value,sort_keys=True).encode()).hexdigest()
    folder=real_path(metadata(build)/'evidence');folder.mkdir(parents=True,exist_ok=True)
    target=real_path(folder/(path.name+'-'+fingerprint[:16]+'.zip'))
    def verify():
        with zipfile.ZipFile(target) as package:
            if set(package.namelist())!={'files/'+n for n in contents}:raise ValueError('Evidence member mismatch')
            for name,data in contents.items():
                if package.read('files/'+name)!=data:raise ValueError('Evidence payload mismatch')
    if target.exists():
        verify()
        return target
    occupied=sum(real_path(p).stat().st_size for p in folder.iterdir() if p.is_file())
    if occupied+total+65536>POLICY['evidencePoolBytes']:raise ValueError('Evidence pool quota exceeded; run daily maintenance')
    with zipfile.ZipFile(target,'x',zipfile.ZIP_DEFLATED,compresslevel=3) as package:
        for name,data in contents.items():relative_member(name);package.writestr('files/'+name,data)
    verify()
    return target


def pack(build, name, record, *, cold=None, deadline=float('inf')):
    path = run_path(build, name)
    value = expected_inventory(record)
    if inventory(path) != value:
        raise ValueError('Task differs from sealed content; resume and close again')
    cold = real_path(cold or POLICY['coldRoot'])
    cold.mkdir(parents=True, exist_ok=True)
    archive = cold/(name+'-'+record['manifestSha256'][:12]+'.zip')
    if not archive.exists():
        stored = cold_usage(cold)
        estimate = value['bytes']+64*2**20
        volume = shutil.disk_usage(cold)
        if stored+estimate > POLICY['coldBytes'] or volume.free-estimate < max(20*GIB, volume.total//10):
            raise ValueError('Cold store quota/reserve would be exceeded')
        temporary = real_path(archive.with_suffix('.partial'))
        if temporary.exists():
            raise ValueError('Incomplete recovery package needs inspection')
        with zipfile.ZipFile(temporary, 'x', zipfile.ZIP_DEFLATED, compresslevel=3) as package:
            package.writestr('inventory.json', json.dumps(value))
            for member in value['files']:
                if time.monotonic() > deadline:
                    raise TimeoutError('Maintenance time budget reached; partial archive retained')
                package.write(path/member, 'files/'+member)
        verify_archive(temporary, value)
        os.replace(temporary, archive)
    sha = verify_archive(archive, value, record.get('archiveSha256'))
    with database(build, True) as db:
        db.execute('BEGIN IMMEDIATE')
        latest = rows(db)[name]
        if latest['status'] != 'closed' or lifecycle(latest) != lifecycle(record):
            raise ValueError('Task lifecycle changed while archiving')
        latest.update(archive=str(archive), archiveSha256=sha)
        save(db, name, latest)
        record.update(archive=str(archive), archiveSha256=sha)
    return value


def lifecycle(record):
    return (record.get('generation',0),record['status'],record.get('closed'),record.get('manifestSha256'))


def protected_names(records):
    return {name for r in records.values() if r['status'] == 'active' for name in r.get('inputs', [])}


def orphan_evidence(build,records,now):
    folder=real_path(metadata(build)/'evidence')
    if not folder.exists():return []
    referenced={r.get('evidence') for r in records.values()}
    found=[]
    for path in folder.glob('*.zip'):
        path=real_path(path)
        if str(path) in referenced or now-path.stat().st_mtime<POLICY['evidenceDays']*86400:continue
        name,separator,fingerprint=path.stem.rpartition('-')
        if separator and name in records and len(fingerprint)==16:found.append(path)
    return found


def status(build):
    start = time.monotonic()
    with database(build) as db:
        records = rows(db)
    root = real_path(real_path(build)/'runs')
    children = list(root.iterdir()) if root.exists() else []
    usage = shutil.disk_usage(build)
    return dict(managedTasks=len(records), residentReservationBytes=reservation(records), poolLimitBytes=POLICY['poolBytes'],
                historicalTasks=sum(p.is_dir() and p.name not in records for p in children),
                rootKeepMarkers=sum((p/'.keep').is_file() for p in children if not is_link(p)),
                activeInputTasks=sorted(protected_names(records)),
                coldStoreBytes=cold_usage(),coldStoreLimitBytes=POLICY['coldBytes'],
                blocked={n:r['blocked'] for n,r in records.items() if r.get('blocked')},
                volumeFreeBytes=usage.free, volumeReserveBytes=max(50*GIB, (usage.total+9)//10),
                recursiveScan=False, seconds=time.monotonic()-start)


def active_processes(engine):
    if os.name != 'nt':
        return ['Process audit is unavailable on this platform']
    script = "$OutputEncoding=[Console]::OutputEncoding=[System.Text.UTF8Encoding]::new($false); Get-CimInstance Win32_Process | Where-Object { $_.Name -match '^(cl|link|ninja|ctest|msbuild|qrenderdoc|renderdoccmd|python|cmake|Azure.*)\\.exe$' } | Select-Object ProcessId,Name,ExecutablePath,CommandLine | ConvertTo-Json -Compress"
    result = subprocess.run(['powershell.exe', '-NoProfile', '-NonInteractive', '-Command', script], capture_output=True, text=True,encoding='utf-8', timeout=10,
                            creationflags=subprocess.CREATE_NO_WINDOW)
    if result.returncode:
        raise ValueError('Could not audit active processes')
    if not result.stdout or not result.stdout.strip():raise ValueError('Process audit returned no inventory')
    data = json.loads(result.stdout)
    data=data if isinstance(data,list) else [data]
    if not any(item.get('ProcessId')==os.getpid() for item in data):raise ValueError('Process audit is incomplete')
    engine = str(Path(engine)).lower().replace('/', '\\')
    busy = []
    for item in data:
        if item['ProcessId'] in (os.getpid(), os.getppid()):
            continue
        name = item['Name'].lower()
        command = ((item.get('ExecutablePath') or '')+' '+(item.get('CommandLine') or '')).lower().replace('/', '\\')
        compiler = name in {'cl.exe','link.exe','ninja.exe','ctest.exe','msbuild.exe','qrenderdoc.exe','renderdoccmd.exe'}
        local = engine in command and (name.startswith('azure') or name in {'python.exe','cmake.exe'})
        if compiler or local:
            busy.append(dict(pid=item['ProcessId'], name=name))
    return busy


def tracked_paths(engine):
    repo = Path(subprocess.check_output(['git','-C',str(engine),'rev-parse','--show-toplevel'], text=True).strip())
    return {(repo/p).resolve() for p in subprocess.check_output(['git','-C',str(repo),'ls-files','-z']).decode('utf-8').split('\0') if p}


def git_state(engine):
    data = subprocess.check_output(['git','-C',str(engine),'diff','--binary'], stderr=subprocess.DEVNULL)
    data += subprocess.check_output(['git','-C',str(engine),'status','--porcelain'], stderr=subprocess.DEVNULL)
    return hashlib.sha256(data).hexdigest()


@contextmanager
def maintenance_lock(build):
    path = real_path(metadata(build)/'artifact-maintenance.lock')
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('a+b') as stream:
        if path.stat().st_size == 0:
            stream.write(b'0');stream.flush()
        stream.seek(0)
        if os.name == 'nt':
            import msvcrt
            msvcrt.locking(stream.fileno(), msvcrt.LK_NBLCK, 1)
        else:
            import fcntl
            fcntl.flock(stream, fcntl.LOCK_EX|fcntl.LOCK_NB)
        yield


def maintain(build, *, apply=False, cold=None, now=None, busy=None, tracked=None, git=None):
    now = time.time() if now is None else now
    build = real_path(build)
    with database(build) as db:
        records = rows(db)
    candidates = []
    references = protected_names(records)
    for name, record in records.items():
        path = run_path(build, name)
        marker = path/'.finished'
        if record['status'] != 'closed' or record.get('blocked') or name in references or not marker.is_file() or (path/'.keep').exists():
            continue
        if now-max(record['closed'], marker.stat().st_mtime) >= record.get('retentionDays',POLICY['retentionDays'])*86400:
            candidates.append(name)
    expired = [n for n,r in records.items() if r['status']=='released' and r.get('archive')
               and r.get('recoveryDays',POLICY['recoveryDays']) is not None
               and now-r['released'] >= r.get('recoveryDays',POLICY['recoveryDays'])*86400
               and n not in references and not r.get('blocked')]
    expired_evidence=[n for n,r in records.items() if r['status']=='released' and r.get('evidence')
                      and now-r['released']>=POLICY['evidenceDays']*86400 and n not in references and not r.get('blocked')]
    orphans=orphan_evidence(build,records,now)
    report = dict(applied=apply, candidates=[str(run_path(build,n)) for n in candidates],
                  expiredRecoveryTasks=expired,expiredEvidenceTasks=expired_evidence,expiredUnreferencedEvidence=[str(p) for p in orphans],items=[],status='preview')
    if not apply or not (candidates or expired or expired_evidence or orphans):
        if apply:report['status']='idle'
        return report
    busy = active_processes(build.parent) if busy is None else busy
    if busy:
        report.update(status='deferred-active-work', activeProcesses=busy)
        return report
    tracked = tracked_paths(build.parent) if tracked is None else tracked
    git = (lambda: git_state(build.parent)) if git is None else git
    before = git()
    deadline = time.monotonic()+POLICY['maintenanceSeconds']
    with maintenance_lock(build):
        # Re-read leases after acquiring the exclusive maintenance lock.
        with database(build) as db:
            current = rows(db)
        references = protected_names(current)
        for name in expired[:POLICY['maximumBatch']]:
            record = current[name]
            archive = real_path(record['archive'])
            owned = real_path(cold or POLICY['coldRoot'])
            expected_name = name+'-'+record['manifestSha256'][:12]+'.zip'
            if archive.parent != owned or archive.name != expected_name or digest(archive) != record['archiveSha256']:
                raise ValueError('Expired recovery package ownership/digest differs')
            with database(build, True) as db:
                db.execute('BEGIN IMMEDIATE')
                refreshed = rows(db);latest = refreshed[name]
                if (name in protected_names(refreshed) or latest['status'] != 'released'
                    or lifecycle(latest) != lifecycle(record) or latest.get('archive') != str(archive)
                    or latest.get('released') != record['released'] or run_path(build,name).exists()):
                    continue
                archive.unlink()
                latest.update(archive=None, archiveExpiredAt=now, expiredArchiveSha256=record['archiveSha256'])
                save(db,name,latest)
            report['items'].append(dict(expiredRecoveryTask=name, recoveryDays=record.get('recoveryDays',POLICY['recoveryDays'])))
        for name in expired_evidence[:max(0,POLICY['maximumBatch']-len(report['items']))]:
            record=current[name];evidence=real_path(record['evidence'])
            if evidence.parent!=metadata(build)/'evidence' or not evidence.name.startswith(name+'-') or digest(evidence)!=record['evidenceSha256']:
                raise ValueError('Expired compact evidence ownership/digest differs')
            with database(build,True) as db:
                db.execute('BEGIN IMMEDIATE');refreshed=rows(db);latest=refreshed[name]
                if (latest['status']!='released' or lifecycle(latest)!=lifecycle(record)
                    or name in protected_names(refreshed) or latest.get('evidence')!=str(evidence) or run_path(build,name).exists()):continue
                evidence.unlink();latest.update(evidence=None,evidenceExpiredAt=now);save(db,name,latest)
            report['items'].append(dict(expiredEvidenceTask=name,evidenceDays=POLICY['evidenceDays']))
        for evidence in orphans[:max(0,POLICY['maximumBatch']-len(report['items']))]:
            with zipfile.ZipFile(evidence) as package:
                entry=package.getinfo('files/.artifact-inventory.json')
                if entry.file_size>POLICY['evidenceBytes']:raise ValueError('Unreferenced evidence exceeds its ownership record')
                original=json.loads(package.read(entry))
                expected=hashlib.sha256(json.dumps(original,sort_keys=True).encode()).hexdigest()[:16]
                if evidence.stem.rsplit('-',1)[-1]!=expected or package.testzip():raise ValueError('Unreferenced evidence ownership differs')
            with database(build,True) as db:
                db.execute('BEGIN IMMEDIATE')
                if str(evidence) in {r.get('evidence') for r in rows(db).values()}:continue
                if now-evidence.stat().st_mtime<POLICY['evidenceDays']*86400:continue
                evidence.unlink()
            report['items'].append(dict(expiredUnreferencedEvidence=str(evidence)))
        for name in candidates[:max(0,POLICY['maximumBatch']-len(report['items']))]:
            if time.monotonic() >= deadline:
                break
            path = run_path(build, name)
            if name in references or (path/'.keep').exists() or current[name]['status'] != 'closed':
                continue
            record = current[name]
            free_before = shutil.disk_usage(build).free
            try:
                if any(p == path or path in p.parents for p in tracked):
                    raise ValueError('Tracked content is protected')
                value=expected_inventory(record)
                evidence=real_path(record['evidence'])
                if evidence.parent!=metadata(build)/'evidence' or digest(evidence)!=record['evidenceSha256']:
                    raise ValueError('Compact evidence ownership/digest differs')
                if record.get('retainRecovery'):
                    value = pack(build, name, record, cold=cold, deadline=deadline)
                if inventory(path) != value or (path/'.keep').exists():
                    raise ValueError('Task changed after recovery verification')
                if git() != before:
                    raise ValueError('Git state changed during maintenance')
                with database(build, True) as db:
                    db.execute('BEGIN IMMEDIATE')
                    latest = rows(db)
                    marker = path/'.finished'
                    if (name in protected_names(latest) or latest[name]['status'] != 'closed'
                        or lifecycle(latest[name]) != lifecycle(record) or (path/'.keep').exists()
                        or not marker.is_file() or now-marker.stat().st_mtime < record.get('retentionDays',POLICY['retentionDays'])*86400):
                        continue
                    # Serializes lease acquisition and resume against removal.
                    remove_artifact(build, path, tracked, apply=True)
                    record.update(status='released', reservedBytes=0, released=time.time())
                    save(db, name, record)
                report['items'].append(dict(path=str(path), archive=record.get('archive'), archiveSha256=record.get('archiveSha256'),evidence=record['evidence'],
                                            logicalBytes=value['bytes'], volumeFreedBytes=shutil.disk_usage(build).free-free_before))
            except (OSError, ValueError, TimeoutError, zipfile.BadZipFile) as error:
                with database(build, True) as db:
                    db.execute('BEGIN IMMEDIATE')
                    latest = rows(db)[name]
                    if latest['status']=='closed' and lifecycle(latest)==lifecycle(record):
                        latest['blocked'] = str(error)
                        save(db, name, latest)
                report['items'].append(dict(path=str(path), blocked=str(error)))
        if git() != before:
            raise ValueError('Git state changed; inspect maintenance receipts')
    report['status'] = 'blocked' if any('blocked' in item for item in report['items']) else 'completed'
    write_json(metadata(build)/'managed-maintenance-latest.json', report)
    return report


def restore(build, name):
    path = run_path(build, name)
    with database(build, True) as db:
        db.execute('BEGIN IMMEDIATE')
        records = rows(db);record = records.get(name)
        if not record or record['status'] != 'released' or path.exists():
            raise ValueError('Restore requires an absent, released managed task')
        if not record.get('archive'):
            raise ValueError('Rebuild generated outputs using retained evidence; no full recovery package is registered')
        value = expected_inventory(record)
        verify_archive(record['archive'], value, record['archiveSha256'])
        if reservation(records)+value['bytes'] > POLICY['poolBytes']:
            raise ValueError('Restore exceeds the managed pool')
        from artifact_budget import evaluate
        usage = shutil.disk_usage(build)
        if not evaluate(total=usage.total, free=usage.free, used=0, planned=value['bytes'])['allowed']:
            raise ValueError('Restore exceeds task/volume budget')
        path.mkdir()
        record.update(status='restoring', reservedBytes=value['bytes'], generation=record.get('generation',0)+1)
        save(db,name,record)
    try:
        with zipfile.ZipFile(record['archive']) as package:
            for directory in value['directories']:
                real_path(path/str(relative_member(directory))).mkdir(parents=True, exist_ok=True)
            for member, item in value['files'].items():
                target = real_path(path/str(relative_member(member)))
                target.parent.mkdir(parents=True, exist_ok=True)
                with package.open('files/'+member) as source, target.open('xb') as output:
                    shutil.copyfileobj(source, output, 1024*1024)
                os.utime(target, ns=(item['mtimeNs'], item['mtimeNs']))
        if inventory(path) != value:
            raise ValueError('Restored bytes differ; preserve the directory for inspection')
        record.update(status='active', reservedBytes=value['bytes'], blocked=None)
    except Exception as error:
        record.update(status='blocked-restore', blocked=str(error))
        with database(build,True) as db:
            save(db,name,record)
        raise
    with database(build,True) as db:
        save(db, name, record)
    return dict(status='restored', path=str(path), missingInputs=[n for n in record.get('inputs',[]) if not run_path(build,n).exists()])


def measure(path):
    total = count = 0
    for current, dirs, names in os.walk(real_path(path), followlinks=False):
        for name in dirs:
            real_path(Path(current)/name)
            if name == '.git':
                raise ValueError('Nested Git worktree is protected')
        for name in names:
            total += real_path(Path(current)/name).stat().st_size
            count += 1
            if total > POLICY['runBytes'] or count > POLICY['maximumFiles']:
                raise ValueError('Task byte/file-count budget exceeded')
    return total


def guarded_run(build, name, command, timeout=600):
    """Monitor a foreground child; sampled quotas are not filesystem quotas."""
    import threading
    from collections import deque
    path = run_path(build,name)
    if not command or timeout <= 0:
        raise ValueError('Expected a command and positive timeout')
    with database(build,True) as db:
        db.execute('BEGIN IMMEDIATE');record=rows(db).get(name)
        if not record or record['status']!='active' or record.get('processPid'):
            raise ValueError('Command requires an active, idle managed task')
        record.update(processPid=-1,generation=record.get('generation',0)+1);save(db,name,record)
    process=None;reader=None;errors=[];truncated=[False];start=time.monotonic();reason=None
    try:
        kwargs=dict(cwd=Path(build).parent,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        if os.name=='nt':kwargs['creationflags']=subprocess.CREATE_NO_WINDOW|subprocess.CREATE_NEW_PROCESS_GROUP
        else:kwargs['start_new_session']=True
        process=subprocess.Popen(command,**kwargs)
        with database(build,True) as db:
            current=rows(db)[name];current['processPid']=process.pid;save(db,name,current)
        def drain():
            tail=deque(maxlen=16);written=0;head_limit=15*2**20-256
            try:
                with real_path(path/'process.log').open('wb') as log:
                    for block in iter(lambda:process.stdout.read(65536),b''):
                        if written<head_limit:
                            keep=block[:head_limit-written];log.write(keep);written+=len(keep)
                            if len(keep)<len(block):truncated[0]=True;tail.append(block[len(keep):])
                        else:truncated[0]=True;tail.append(block)
                    if truncated[0]:
                        log.write(b'\n--- log capped at 16 MiB; retained tail ---\n')
                        for block in tail:log.write(block)
            except Exception as error:errors.append(str(error))
        reader=threading.Thread(target=drain,daemon=True);reader.start()
        from artifact_budget import check
        while process.poll() is None:
            try:
                measure(path)
                result=check(build,run=path,planned=0,diagnostic=json.loads((path/'budget.json').read_text()).get('diagnostic',False))
                if not result['allowed']:raise ValueError(','.join(result['reasons']))
                if errors:raise ValueError(errors[0])
                if time.monotonic()-start>timeout:raise TimeoutError('Owned command time budget exceeded')
            except (OSError,ValueError,TimeoutError) as error:
                reason=str(error)
                if os.name=='nt':
                    subprocess.run(['taskkill.exe','/PID',str(process.pid),'/T','/F'],capture_output=True,creationflags=subprocess.CREATE_NO_WINDOW)
                else:
                    import signal
                    os.killpg(process.pid,signal.SIGKILL)
                break
            time.sleep(1)
        process.wait(timeout=10);reader.join(timeout=10)
        if reader.is_alive():reason=reason or 'Foreground command left an output pipe open'
        if errors:reason=reason or errors[0]
        if not reason:
            try:
                measure(path)
                final=check(build,run=path,planned=0,diagnostic=json.loads((path/'budget.json').read_text()).get('diagnostic',False))
                if not final['allowed']:reason=','.join(final['reasons'])
            except (OSError,ValueError) as error:reason=str(error)
    finally:
        if process and process.stdout:process.stdout.close()
        with database(build,True) as db:
            current=rows(db)[name];current['processPid']=None;save(db,name,current)
    report=dict(status='budget-stopped' if reason else 'exited',exitCode=process.returncode,reason=reason,
                seconds=time.monotonic()-start,logTruncated=truncated[0],samplingSeconds=1,command=command)
    write_json(metadata(build)/'process-receipts'/(name+'.json'),report)
    return report


def daily(build,apply=False):
    start=time.monotonic()
    busy=active_processes(Path(build).parent) if apply else []
    result=(dict(status='deferred-active-work',activeProcesses=busy,applied=apply,items=[])
            if busy else maintain(build,apply=apply,busy=busy))
    report=dict(managed=result,caches=None,pipelineCaches=None,externalStorageUsed=False)
    if result['status']!='deferred-active-work':
        from artifact_pipeline_cache import clean as clean_pipelines
        remaining=POLICY['maintenanceSeconds']-(time.monotonic()-start)
        if remaining>5:
            report['pipelineCaches']=clean_pipelines(build,apply=apply,seconds=min(10,remaining))
    policy_path=metadata(build)/'cache-cleanup-policy.json'
    if apply and result['status']!='deferred-active-work' and policy_path.is_file():
        policy=json.loads(real_path(policy_path).read_text(encoding='utf-8'))
        remaining=POLICY['maintenanceSeconds']-(time.monotonic()-start)
        if remaining>5:
            from artifact_cache_cleanup import clean
            cache=clean(Path(build).parent,Path(policy['catalog']),Path(policy['lock']),apply=True,
                        limit_bytes=min(int(policy.get('maximumBytes',10*GIB)),10*GIB),seconds=remaining)
            report['caches']={k:v for k,v in cache.items() if k!='items'}|dict(files=len(cache['items']))
    report['seconds']=time.monotonic()-start
    write_json(metadata(build)/'daily-maintenance-latest.json',report)
    return report


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('mode', choices=['status','maintain','daily','resume','restore','run'])
    parser.add_argument('--build', type=Path, default=Path(__file__).resolve().parents[1]/'build')
    parser.add_argument('--run')
    parser.add_argument('--apply', action='store_true')
    parser.add_argument('--timeout',type=float,default=600)
    parser.add_argument('--command',nargs=argparse.REMAINDER)
    args = parser.parse_args()
    try:
        if args.mode == 'status':
            result = status(args.build)
        elif args.mode == 'maintain':
            result = maintain(args.build, apply=args.apply)
        elif args.mode=='daily':result=daily(args.build,args.apply)
        elif args.mode=='run':
            if not args.run or not args.command:parser.error('run requires --run and --command')
            result=guarded_run(args.build,args.run,args.command,args.timeout)
        else:
            if not args.run:
                parser.error('--run requires a task directory name')
            result = restore(args.build,args.run) if args.mode == 'restore' else resume(args.build,args.run) or dict(status='resumed')
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return 2 if result.get('status') in {'blocked','budget-stopped'} else result.get('exitCode',0)
    except (OSError, ValueError, sqlite3.Error, zipfile.BadZipFile) as error:
        parser.exit(2, str(error)+'\n')


if __name__ == '__main__':
    raise SystemExit(main())
