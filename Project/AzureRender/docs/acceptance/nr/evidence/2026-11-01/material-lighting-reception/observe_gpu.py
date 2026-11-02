import argparse,hashlib,json,os,runpy,shutil,subprocess,sys,time
from pathlib import Path
parser=argparse.ArgumentParser();parser.add_argument('--tool',type=Path,required=True);parser.add_argument('--audit',type=Path,required=True);parser.add_argument('arguments',nargs=argparse.REMAINDER);args=parser.parse_args();tool=args.tool.resolve();arguments=args.arguments[1:] if args.arguments[:1]==['--'] else args.arguments;original=subprocess.run;records=[];status='running'
def observed(command,*positional,**kwargs):
    player=isinstance(command,(list,tuple)) and command and Path(command[0]).name.lower()=='azureplayer.exe';start=time.time_ns();result=None
    cwd=Path(kwargs.get('cwd') or Path.cwd()).resolve();source=cwd/'captures/azureplayer.log.jsonl'
    previous=source.read_bytes() if player and source.exists() else b''
    try:
        result=original(command,*positional,**kwargs);return result
    finally:
        if player:
            cwd=Path(kwargs.get('cwd') or Path.cwd()).resolve();source=cwd/'captures/azureplayer.log.jsonl';capture=Path(command[command.index('--capture-dir')+1]);capture=capture if capture.is_absolute() else cwd/capture;destination=capture.parent/'diagnostics/azureplayer.log.jsonl';destination.parent.mkdir(parents=True,exist_ok=True);row=dict(command=[str(x) for x in command],exitCode=result.returncode if result is not None else None,structuredLog=str(destination));records.append(row)
            if source.exists():
                assert source.stat().st_mtime_ns>=start-2_000_000_000,'Structured log predates this Player invocation'
                current=source.read_bytes();appended=current.startswith(previous);fresh=current[len(previous):] if appended else current
                assert fresh,'Successful Player invocation has no new structured log records'
                destination.write_bytes(fresh);rows=[json.loads(line) for line in fresh.decode('utf-8').splitlines()];row['structuredLogSha256']=hashlib.sha256(fresh).hexdigest();row['logScope']=dict(previousBytes=len(previous),appended=appended,currentInvocationBytes=len(fresh),firstTimestampMs=rows[0]['timestamp_ms'],lastTimestampMs=rows[-1]['timestamp_ms'])
                if result is not None and result.returncode==0:
                    assert not any(r['level']=='error' or r['subsystem']=='validation' and r['level']=='warning' for r in rows);assert any(r['message']=='Allocator after unload: buffers=0 images=0' for r in rows);row['released']=True
            else:assert result is None or result.returncode!=0,'Successful Player invocation has no structured log'
subprocess.run=observed;sys.path.insert(0,str(tool.parent));sys.argv=[str(tool),*arguments]
try:
    try:runpy.run_path(str(tool),run_name='__main__')
    except SystemExit as error:
        if error.code not in (None,0):raise
    assert records and all(r.get('released') for r in records);status='passed'
finally:
    args.audit.parent.mkdir(parents=True,exist_ok=True);args.audit.write_bytes((json.dumps(dict(status=status if status=='passed' else 'failed',tool=str(tool),toolSha256=hashlib.sha256(tool.read_bytes()).hexdigest(),players=records),indent=2)+'\n').encode())
