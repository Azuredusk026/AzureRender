"""Discard redundant AZCACHE1 byte copies, preserving canonical mounted inputs."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import time

from artifact_maintenance import real_path,write_json,active_processes,tracked_paths,digest,maintenance_lock


def identity(path):
    stat=path.stat()
    return [stat.st_size,stat.st_mtime_ns,stat.st_ino,stat.st_dev]


def payload_digest(path,deadline):
    before=identity(path)
    with path.open('rb') as stream:
        header=stream.read(24)
        if len(header)!=24 or header[:8]!=b'AZCACHE1' or struct.unpack('<Q',header[16:])[0]!=before[0]-24:
            raise ValueError('Invalid generated cache header')
        if not path.stem.isdecimal() or int(path.stem)!=struct.unpack('<Q',header[8:16])[0]:
            raise ValueError('Cache fingerprint differs from its file name')
        h=hashlib.sha256()
        for block in iter(lambda:stream.read(1024*1024),b''):
            if time.monotonic()>deadline:raise TimeoutError('Cache maintenance time budget reached')
            h.update(block)
    if identity(path)!=before:raise ValueError('Cache changed during inspection')
    return header.hex(),h.hexdigest(),before


def _clean(engine,catalog,lock,*,apply=False,limit_bytes=10*2**30,seconds=120,busy=None,tracked=None):
    start=time.monotonic();deadline=start+seconds
    engine=real_path(engine);build=engine/'build';catalog=real_path(catalog);lock=real_path(lock)
    day=time.strftime('%Y-%m-%d');ledger_path=build/'maintenance/cache-cleanup-daily-budget.json'
    ledger=json.loads(ledger_path.read_text(encoding='utf-8')) if ledger_path.exists() else {}
    used=ledger.get('reservedBytes',0) if ledger.get('date')==day else 0
    if type(used) is not int or not 0<=used<=10*2**30:raise ValueError('Invalid daily cache budget')
    limit_bytes=min(limit_bytes,10*2**30-used)
    if limit_bytes<24:return dict(status='daily-budget-used',items=[],logicalBytes=0,externalStorageUsed=False)
    info=json.loads(catalog.read_text(encoding='utf-8'));root=real_path(info['root'])
    if root.parent!=build/'runs' or not root.is_dir():raise ValueError('Expected a direct historical task root')
    frozen=json.loads(lock.read_text(encoding='utf-8'))['project']
    projects=[real_path(p) for p in frozen if p.endswith('.azureproject')]
    if not projects:raise ValueError('Current mounted project input lock is required')
    protected=[p.parent/'.azure/cache' for p in projects]
    canonical={}
    for name,sha in frozen.items():
        path=real_path(name)
        if path.is_file() and path.suffix!='.azcache':canonical.setdefault(sha,path)
    busy=active_processes(engine) if busy is None else busy
    if busy:return dict(status='deferred-active-work',items=[],activeProcesses=busy)
    tracked=tracked_paths(engine) if tracked is None else tracked
    progress_path=build/'maintenance'/(catalog.stem+'-proofs.json')
    progress=json.loads(progress_path.read_text(encoding='utf-8')) if progress_path.exists() else {}
    verified_sources={};items=[];bytes_removed=0;reused=0;unknown=0;budget_skipped=0
    import shutil
    free_before=shutil.disk_usage(engine).free
    for row in sorted(info['items'],key=lambda r:r['bytes'],reverse=True):
        if time.monotonic()>=deadline or bytes_removed>=limit_bytes:break
        if type(row['bytes']) is not int or row['bytes']<0:raise ValueError('Invalid cache catalog size')
        if row['bytes']>limit_bytes-bytes_removed:
            budget_skipped+=1;continue
        path=real_path(row['path'])
        if root not in path.parents or path.suffix!='.azcache' or '.azure' not in path.parts:raise ValueError('Unexpected cache target')
        azure=next(p for p in path.parents if p.name=='.azure')
        if path.parent.parent!=azure/'cache':raise ValueError('Expected .azure/cache/<asset-id>/<fingerprint>.azcache')
        if any(p==path or p in path.parents for p in protected) or path in tracked or not path.is_file():continue
        if any((p/'.git').exists() for p in path.parents if p==root or root in p.parents):raise ValueError('Nested Git worktree is protected')
        current=identity(path)
        if current[0]>limit_bytes-bytes_removed:
            budget_skipped+=1;continue
        if time.time()-path.stat().st_mtime<86400:continue
        previous=progress.get(str(path))
        if previous and previous['identity']==current and previous.get('unmatched'):
            reused+=1;continue
        try:header,sha,current=payload_digest(path,deadline)
        except TimeoutError:break
        except ValueError as error:
            progress[str(path)]=dict(identity=current,unmatched=True,reason=str(error));continue
        source=canonical.get(sha)
        if not source:
            progress[str(path)]=dict(identity=current,unmatched=True,payloadSha256=sha);unknown+=1;continue
        if str(source) not in verified_sources:
            before=identity(source)
            if digest(source)!=sha or identity(source)!=before:raise ValueError('Frozen canonical source differs')
            verified_sources[str(source)]=before
        if identity(source)!=verified_sources[str(source)] or identity(path)!=current:raise ValueError('Source/cache changed before removal')
        if bytes_removed+current[0]>limit_bytes:continue
        item=dict(path=str(path),bytes=current[0],headerHex=header,canonicalSource=str(source),payloadSha256=sha,sourceIdentity=verified_sources[str(source)],cacheIdentity=current,applied=False)
        items.append(item)
        # Persist the exact file and surviving source before mutation.
        write_json(build/'maintenance'/'cache-cleanup-current-batch.json',dict(items=items[-10:],inputLock=str(lock)))
        if apply:
            # Reserve before unlink so interruption cannot grant another full daily slice.
            write_json(ledger_path,dict(date=day,reservedBytes=used+bytes_removed+current[0],maximumBytes=10*2**30))
            real_path(path).unlink();item['applied']=True
        bytes_removed+=current[0]
        if len(items)%10==0:write_json(progress_path,progress)
    write_json(progress_path,progress)
    result=dict(status='completed' if apply else 'preview',applied=apply,items=items,
                logicalBytes=bytes_removed,volumeFreedBytes=shutil.disk_usage(engine).free-free_before,
                unmatched=unknown,reusedUnmatchedProofs=reused,canonicalSourcesVerified=len(verified_sources),
                skippedForRemainingBudget=budget_skipped,
                sourceLock=str(lock),catalog=str(catalog),wholeRunBackupCreated=False,externalStorageUsed=False,
                seconds=time.monotonic()-start)
    write_json(build/'maintenance'/'cache-cleanup-latest.json',result)
    return result


def clean(engine,catalog,lock,**kwargs):
    with maintenance_lock(Path(engine)/'build'):
        return _clean(engine,catalog,lock,**kwargs)


def main():
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('--catalog',type=Path,required=True);parser.add_argument('--lock',type=Path,required=True)
    parser.add_argument('--apply',action='store_true');parser.add_argument('--gib',type=float,default=10);parser.add_argument('--seconds',type=float,default=120)
    args=parser.parse_args()
    if not 0<args.gib<=10 or not 0<args.seconds<=120:parser.error('Daily slice is at most 10 GiB and 120 seconds')
    try:
        result=clean(Path(__file__).resolve().parents[1],args.catalog,args.lock,apply=args.apply,limit_bytes=int(args.gib*2**30),seconds=args.seconds)
        print(json.dumps({k:v for k,v in result.items() if k!='items'}|dict(files=len(result['items'])),ensure_ascii=False,indent=2))
    except (OSError,ValueError) as error:parser.exit(2,str(error)+'\n')


if __name__=='__main__':main()
