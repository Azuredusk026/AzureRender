"""Bounded daily removal of verified historical model byte copies."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shutil
import subprocess
import time

from artifact_cleanup import is_link
from artifact_maintenance import real_path,write_json,active_processes,tracked_paths,maintenance_lock

MAXIMUM_BYTES=32*2**30
MAXIMUM_FILES=512
MAXIMUM_ROWS=50000
MAXIMUM_METADATA=32*2**20


def identity(path):
    value=path.stat()
    return [value.st_size,value.st_mtime_ns,value.st_ino,value.st_dev]


def file_digest(path,deadline):
    before=identity(path);value=hashlib.sha256()
    with path.open('rb') as stream:
        while True:
            if time.monotonic()>=deadline:raise TimeoutError('Model maintenance deadline reached')
            block=stream.read(1024*1024)
            if not block:break
            value.update(block)
    if identity(path)!=before:raise ValueError('Model changed while hashing')
    return value.hexdigest(),before


def read_json(path):
    path=real_path(path)
    if path.stat().st_size>MAXIMUM_METADATA:raise ValueError('Model maintenance metadata exceeds 32 MiB')
    return json.loads(path.read_text(encoding='utf-8'))


def inside(engine,path):
    path=real_path(path)
    if engine not in path.parents:raise ValueError('Model maintenance path is outside the project')
    return path


def history_root(engine,path):
    path=inside(engine,path)
    if path.parent!=engine/'build/runs' or not path.is_dir():raise ValueError('Expected a direct declared historical task root')
    return path


def nested_git(root,path):
    return any((parent/'.git').exists() for parent in (path,*path.parents) if parent==root or root in parent.parents)


def frozen_sources(engine,lock):
    frozen=read_json(lock).get('project')
    if not isinstance(frozen,dict) or len(frozen)>MAXIMUM_ROWS:raise ValueError('Invalid current project input lock')
    protected=set();signatures={};has_project=False
    for name,sha in frozen.items():
        path=Path(name).absolute()
        if '..' in path.parts or engine not in path.parents:raise ValueError('Frozen input is outside the project')
        protected.add(path);has_project|=path.suffix=='.azureproject'
        if path.suffix.lower()!='.glb':continue
        path=inside(engine,path)
        if not isinstance(sha,str) or not re.fullmatch('[0-9a-f]{64}',sha) or not path.is_file():raise ValueError('Frozen model source is missing or invalid')
        signatures.setdefault((path.name.casefold(),path.stat().st_size),[]).append((sha,path))
    if not has_project:raise ValueError('Current mounted project input lock is required')
    return protected,signatures


def create_catalog(engine,root,lock,catalog,*,seconds=120,busy=None):
    engine=real_path(engine);root=history_root(engine,root);lock=inside(engine,lock);catalog=inside(engine,catalog)
    if catalog.parent!=engine/'build/maintenance':raise ValueError('Model catalog belongs in project maintenance')
    if not isinstance(seconds,(int,float)) or isinstance(seconds,bool) or not math.isfinite(seconds) or not 0<seconds<=120:raise ValueError('Inventory deadline is within (0,120] seconds')
    busy=active_processes(engine) if busy is None else busy
    if busy:return dict(status='deferred-active-work',activeProcesses=busy,files=0)
    start=time.monotonic();deadline=start+seconds;protected,signatures=frozen_sources(engine,lock);items=[];sources_seconds=time.monotonic()-start
    program=shutil.which('rg')
    if not program:raise ValueError('Model inventory requires rg for bounded native file listing')
    listed=subprocess.run([program,'--files','--hidden','--no-ignore','--iglob','*.glb','--glob','!**/.azure/**','--glob','!**/.git/**',str(root)],
                          capture_output=True,timeout=max(.01,deadline-time.monotonic()),creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
    if listed.returncode not in (0,1):raise ValueError('Native model listing failed')
    if len(listed.stdout)>MAXIMUM_METADATA:raise ValueError('Native model listing exceeds 32 MiB')
    listing_seconds=time.monotonic()-start-sources_seconds;checked={};known_names={name for name,size in signatures}
    for name in listed.stdout.decode('utf-8').splitlines():
        if time.monotonic()>=deadline:raise TimeoutError('Model catalog deadline reached')
        path=Path(name).absolute()
        if '..' in path.parts or root not in path.parents:raise ValueError('Native model listing left declared history')
        if path.name.casefold() not in known_names or path in protected:continue
        if path.parent not in checked:checked[path.parent]=not nested_git(root,real_path(path.parent))
        if not checked[path.parent]:continue
        if is_link(path):raise ValueError('Linked model copy is protected')
        size=path.stat().st_size
        if (path.name.casefold(),size) not in signatures:continue
        items.append(dict(path=str(path),bytes=size))
        if len(items)>MAXIMUM_ROWS:raise ValueError('Model catalog exceeds 50000 entries')
    value=dict(schemaVersion=1,kind='generated-model-copies',root=str(root),inputLock=str(lock),items=items)
    if len(json.dumps(value).encode('utf-8'))>MAXIMUM_METADATA:raise ValueError('Model catalog exceeds 32 MiB')
    write_json(catalog,value)
    return dict(status='catalogued',files=len(items),logicalBytes=sum(r['bytes'] for r in items),catalog=str(catalog),sourcesSeconds=sources_seconds,listingSeconds=listing_seconds,seconds=time.monotonic()-start)


def _clean(engine,catalog,lock,*,apply=False,limit_bytes=MAXIMUM_BYTES,maximum_files=MAXIMUM_FILES,seconds=120,busy=None,tracked=None):
    if type(limit_bytes) is not int or not 0<limit_bytes<=MAXIMUM_BYTES or type(maximum_files) is not int or not 0<maximum_files<=MAXIMUM_FILES:raise ValueError('Daily model limit is at most 32 GiB and 512 files')
    if not isinstance(seconds,(int,float)) or isinstance(seconds,bool) or not math.isfinite(seconds) or not 0<seconds<=120:raise ValueError('Model deadline is within (0,120] seconds')
    start=time.monotonic();deadline=start+seconds;engine=real_path(engine);build=engine/'build';maintenance=build/'maintenance'
    busy=active_processes(engine) if busy is None else busy
    if busy:return dict(status='deferred-active-work',items=[],activeProcesses=busy,externalStorageUsed=False)
    day=time.strftime('%Y-%m-%d');ledger_path=maintenance/'asset-copy-cleanup-daily-budget.json'
    ledger=read_json(ledger_path) if ledger_path.exists() else {}
    used=ledger.get('reservedBytes',0) if ledger.get('date')==day else 0;used_files=ledger.get('reservedFiles',0) if ledger.get('date')==day else 0
    if type(used) is not int or not 0<=used<=MAXIMUM_BYTES or type(used_files) is not int or not 0<=used_files<=MAXIMUM_FILES:raise ValueError('Invalid persisted model budget')
    remaining_bytes=limit_bytes-used;remaining_files=maximum_files-used_files
    if remaining_bytes<=0 or remaining_files<=0:return dict(status='daily-budget-used',items=[],logicalBytes=0,externalStorageUsed=False)
    catalog=inside(engine,catalog);lock=inside(engine,lock);info=read_json(catalog)
    if info.get('schemaVersion')!=1 or info.get('kind')!='generated-model-copies' or not isinstance(info.get('items'),list) or len(info['items'])>MAXIMUM_ROWS:raise ValueError('Invalid model catalog')
    if any(not isinstance(row,dict) or type(row.get('bytes')) is not int or row['bytes']<0 for row in info['items']):raise ValueError('Invalid model catalog byte count')
    if not any(row['bytes']<=remaining_bytes for row in info['items']):
        return dict(status='daily-budget-used',items=[],logicalBytes=0,skippedForRemainingBudget=len(info['items']),externalStorageUsed=False)
    root=history_root(engine,info['root']);lock_identity=identity(lock);protected,signatures=frozen_sources(engine,lock)
    lock_sha=hashlib.sha256(lock.read_bytes()).hexdigest()
    if identity(lock)!=lock_identity:raise ValueError('Current input lock changed during inspection')
    tracked=tracked_paths(engine) if tracked is None else tracked
    progress_path=maintenance/(catalog.stem+'-source-map.json');progress=read_json(progress_path) if progress_path.exists() else {}
    if not isinstance(progress,dict) or len(progress)>MAXIMUM_ROWS:raise ValueError('Invalid model source map')
    progress_bytes=len(json.dumps(progress,indent=2,ensure_ascii=False).encode('utf-8'))+1
    def map_room(key,value):
        amount=len(json.dumps({key:value},indent=2,ensure_ascii=False).encode('utf-8'))+256
        return amount if progress_bytes+amount<=MAXIMUM_METADATA and (key in progress or len(progress)<MAXIMUM_ROWS) else 0
    items=[];bytes_removed=0;verified_sources={};unmatched=0;skipped=0;free_before=shutil.disk_usage(engine).free
    hashed_identities={};reused_identities=0
    def proved_digest(path):
        nonlocal reused_identities
        current=identity(path);key=tuple(current)
        if key in hashed_identities:
            reused_identities+=1;return hashed_identities[key],current
        sha,current=file_digest(path,deadline);hashed_identities[tuple(current)]=sha;return sha,current
    seen=set()
    for row in sorted(info['items'],key=lambda r:r['bytes'],reverse=True):
        if time.monotonic()>=deadline or len(items)>=remaining_files:break
        if row['bytes']>remaining_bytes-bytes_removed:skipped+=1;continue
        path=inside(engine,row['path'])
        if root not in path.parents or path.suffix.lower()!='.glb' or '.azure' in path.relative_to(root).parts:raise ValueError('Unexpected historical model target')
        if path in seen:continue
        seen.add(path)
        if path in protected or path in tracked or not path.is_file() or nested_git(root,path):continue
        current=identity(path)
        if current[0]!=row['bytes'] or time.time()-path.stat().st_mtime<86400:continue
        candidates=signatures.get((path.name.casefold(),current[0]),[])
        if not candidates:continue
        previous=progress.get(str(path))
        if previous and previous.get('unmatched') and previous.get('identity')==current and previous.get('inputLockSha256')==lock_sha:unmatched+=1;continue
        try:sha,current=proved_digest(path)
        except TimeoutError:break
        source=next((source for expected,source in candidates if expected==sha and source!=path),None)
        if source is None:
            value=dict(identity=current,unmatched=True,inputLockSha256=lock_sha);amount=map_room(str(path),value)
            if not amount:break
            progress[str(path)]=value;progress_bytes+=amount;unmatched+=1;continue
        if str(source) not in verified_sources:
            try:actual,source_identity=proved_digest(source)
            except TimeoutError:break
            if actual!=sha:raise ValueError('Frozen canonical model changed')
            verified_sources[str(source)]=source_identity
        if identity(source)!=verified_sources[str(source)] or identity(path)!=current or identity(lock)!=lock_identity:raise ValueError('Model copy, source or input lock changed before removal')
        mapping=dict(canonicalSource=str(source),sha256=sha,bytes=current[0],discarded=True);amount=map_room(str(path),mapping)
        if not amount:break
        item=dict(path=str(path),bytes=current[0],canonicalSource=str(source),sha256=sha,sourceIdentity=verified_sources[str(source)],copyIdentity=current,copyLinkCount=path.stat().st_nlink,applied=False)
        items.append(item);write_json(maintenance/'asset-copy-cleanup-current-batch.json',dict(items=items[-10:],inputLock=str(lock)))
        if apply:
            write_json(ledger_path,dict(date=day,reservedBytes=used+bytes_removed+current[0],reservedFiles=used_files+len(items),maximumBytes=limit_bytes,maximumFiles=maximum_files))
            # The fully resolved exact file and its surviving source are verified before unlink.
            path=inside(engine,path)
            if identity(path)!=current or identity(source)!=verified_sources[str(source)] or identity(lock)!=lock_identity:raise ValueError('Model or input lock changed after journal publication')
            path.unlink();item['applied']=True
            progress[str(path)]=mapping;progress_bytes+=amount
        bytes_removed+=current[0]
        if len(items)%10==0:write_json(progress_path,progress)
    if len(json.dumps(progress).encode('utf-8'))>MAXIMUM_METADATA:raise ValueError('Model source map exceeds 32 MiB')
    write_json(progress_path,progress)
    result=dict(status='completed' if apply else 'preview',applied=apply,items=items,logicalBytes=bytes_removed,
                volumeFreedBytes=shutil.disk_usage(engine).free-free_before,unmatched=unmatched,skippedForRemainingBudget=skipped,
                canonicalSourcesVerified=len(verified_sources),catalog=str(catalog),sourceLock=str(lock),
                payloadFilesHashed=len(hashed_identities),reusedFileIdentityProofs=reused_identities,
                sourceMap=str(progress_path),externalStorageUsed=False,wholeRunBackupCreated=False,seconds=time.monotonic()-start)
    write_json(maintenance/'asset-copy-cleanup-latest.json',result);return result


def clean(engine,catalog,lock,**kwargs):
    with maintenance_lock(Path(engine)/'build'):return _clean(engine,catalog,lock,**kwargs)


def main():
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('mode',choices=['inventory','clean'])
    parser.add_argument('--catalog',required=True,type=Path);parser.add_argument('--lock',required=True,type=Path);parser.add_argument('--root',type=Path)
    parser.add_argument('--apply',action='store_true');parser.add_argument('--gib',type=float,default=32);parser.add_argument('--files',type=int,default=512);parser.add_argument('--seconds',type=float,default=120)
    args=parser.parse_args();engine=Path(__file__).resolve().parents[1]
    try:
        if args.mode=='inventory':
            if args.root is None:parser.error('inventory requires --root')
            result=create_catalog(engine,args.root,args.lock,args.catalog,seconds=args.seconds)
        else:
            if not math.isfinite(args.gib) or not 0<args.gib<=32:raise ValueError('Daily model budget is within (0,32] GiB')
            result=clean(engine,args.catalog,args.lock,apply=args.apply,limit_bytes=int(args.gib*2**30),maximum_files=args.files,seconds=args.seconds)
        print(json.dumps({k:v for k,v in result.items() if k!='items'}|({'files':len(result['items'])} if 'items' in result else {}),ensure_ascii=False,indent=2))
    except (OSError,ValueError,TimeoutError,subprocess.TimeoutExpired) as error:parser.exit(2,str(error)+'\n')


if __name__=='__main__':main()
