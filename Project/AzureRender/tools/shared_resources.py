"""Shared heavy-work exclusion and main-index leases for external consumers."""
import argparse
from contextlib import contextmanager
import json
import os
from pathlib import Path
import sqlite3
import time
import uuid

import artifact_maintenance as maintenance

DEFAULT_BUILD = Path('D:/Projects/FYP/Project/AzureRender/build')
DEFAULT_QUEUE = Path('D:/Projects/FYP-coordination/resources')


def process_alive(pid):
    if type(pid) is not int or pid<=0:
        raise ValueError('Unknown resource process requires inspection')
    if os.name=='nt':
        import ctypes
        from ctypes import wintypes
        kernel=ctypes.WinDLL('kernel32',use_last_error=True)
        kernel.OpenProcess.argtypes=[wintypes.DWORD,wintypes.BOOL,wintypes.DWORD]
        kernel.OpenProcess.restype=wintypes.HANDLE
        kernel.GetExitCodeProcess.argtypes=[wintypes.HANDLE,ctypes.POINTER(wintypes.DWORD)]
        kernel.CloseHandle.argtypes=[wintypes.HANDLE]
        handle=kernel.OpenProcess(0x1000,False,pid)
        if not handle:
            if ctypes.get_last_error()==87:return False
            return True  # Access denied remains conservatively occupied.
        try:
            code=wintypes.DWORD()
            if not kernel.GetExitCodeProcess(handle,ctypes.byref(code)):
                raise OSError('Could not inspect resource process')
            return code.value==259
        finally:
            kernel.CloseHandle(handle)
    try:
        os.kill(pid,0)
        return True
    except ProcessLookupError:
        return False
    except PermissionError:
        return True


def check_previous_owner(queue):
    path=queue/'current.json'
    if not path.exists():return
    previous=json.loads(path.read_text())
    if previous.get('state')!='held':return
    if process_alive(previous.get('pid')):
        raise ValueError('Previous resource owner is still running')
    output=maintenance.real_path(previous['output'])
    if output.parent.name=='runs':
        build=output.parent.parent
        try:
            with maintenance.database(build) as db:
                record=maintenance.rows(db).get(output.name,{})
        except sqlite3.OperationalError as error:
            if error.sqlite_errorcode!=sqlite3.SQLITE_READONLY_ROLLBACK:
                raise
            # A terminated writer can leave a journal that requires a writable connection.
            with maintenance.maintenance_lock(build), maintenance.database(build, True) as db:
                record=maintenance.rows(db).get(output.name,{})
        child=record.get('processPid')
        if child and (child==-1 or process_alive(child)):
            raise ValueError('An orphaned managed command is running or requires inspection')


@contextmanager
def lease(queue, owner, task, output):
    queue = maintenance.real_path(queue)
    queue.mkdir(parents=True, exist_ok=True)
    token = uuid.uuid4().hex
    try:
        with maintenance.maintenance_lock(queue):
            check_previous_owner(queue)
            value = dict(owner=owner, task=task, output=str(Path(output).absolute()),
                         pid=os.getpid(), token=token, started=time.time(), state='held')
            maintenance.write_json(queue/'current.json', value)
            try:
                yield value
            finally:
                value.update(state='released', ended=time.time())
                maintenance.write_json(queue/'current.json', value)
    except OSError as error:
        raise ValueError('Shared resource busy or inaccessible: '+str(error)) from error


def try_once(queue, owner, task, output):
    with lease(queue, owner, task, output):
        return True


def acquire_inputs(build, consumer, inputs, owner):
    consumer = maintenance.real_path(consumer)
    if not consumer.is_dir() or not inputs or not owner:
        raise ValueError('Expected existing consumer directory, inputs and owner')
    names = [maintenance.run_path(build, name).name for name in inputs]
    token = 'external-input-'+uuid.uuid4().hex
    # Same transaction boundary as removal: cleanup cannot pass this reference.
    with maintenance.maintenance_lock(build), maintenance.database(build, True) as db:
        db.execute('BEGIN IMMEDIATE')
        records = maintenance.rows(db)
        if any(not maintenance.run_path(build, name).is_dir() or
               records.get(name, {}).get('status')=='released' for name in names):
            raise ValueError('Input must be a resident task')
        maintenance.save(db, token, dict(status='active', reservedBytes=0,
            inputs=names, externalConsumer=str(consumer), owner=owner,
            created=time.time(), processPid=None, generation=1, blocked=None))
    return token


def release_inputs(build, token, owner):
    with maintenance.database(build, True) as db:
        db.execute('BEGIN IMMEDIATE')
        record = maintenance.rows(db).get(token)
        if not record or not record.get('externalConsumer') or record.get('owner')!=owner:
            raise ValueError('External input lease owner differs or lease is absent')
        db.execute('DELETE FROM runs WHERE name=?', (token,))


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('mode', choices=['run','inputs','release-inputs','status'])
    parser.add_argument('--queue', type=Path, default=DEFAULT_QUEUE)
    parser.add_argument('--build', type=Path, default=DEFAULT_BUILD)
    parser.add_argument('--owner', default=os.environ.get('CODEX_THREAD_ID'))
    parser.add_argument('--task', default='heavy-work')
    parser.add_argument('--run')
    parser.add_argument('--consumer', type=Path)
    parser.add_argument('--input-run', action='append', default=[])
    parser.add_argument('--token')
    parser.add_argument('--timeout', type=float, default=600)
    parser.add_argument('--command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    try:
        if args.mode=='status':
            path = args.queue/'current.json'
            result = json.loads(path.read_text()) if path.exists() else dict(state='unused')
        elif not args.owner:
            parser.error('--owner or CODEX_THREAD_ID is required')
        elif args.mode=='inputs':
            if not args.consumer:parser.error('--consumer is required')
            result = dict(token=acquire_inputs(args.build,args.consumer,args.input_run,args.owner))
        elif args.mode=='release-inputs':
            release_inputs(args.build,args.token,args.owner)
            result = dict(state='released')
        else:
            if not args.run or not args.command:parser.error('--run and --command are required')
            output = maintenance.run_path(args.build,args.run)
            with lease(args.queue,args.owner,args.task,output):
                result = maintenance.guarded_run(args.build,args.run,args.command,args.timeout)
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return 2 if result.get('status')=='budget-stopped' else result.get('exitCode',0)
    except (ValueError,OSError) as error:
        parser.exit(2,str(error)+'\n')


if __name__=='__main__':
    raise SystemExit(main())
