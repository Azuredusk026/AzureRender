"""Record returned NT calls in a suspended child created by this script only."""
import argparse,ctypes as c,gzip,hashlib,json,os,struct,subprocess,time,shutil,sys
from ctypes import wintypes as w
from pathlib import Path

k=c.WinDLL('kernel32',use_last_error=True)
def signature(name,args,result):
 f=getattr(k,name);f.argtypes=args;f.restype=result;return f
HANDLE=c.c_void_p;PTR=c.c_void_p;SIZE=c.c_size_t
OpenProcess=signature('OpenProcess',[w.DWORD,w.BOOL,w.DWORD],HANDLE)
OpenThread=signature('OpenThread',[w.DWORD,w.BOOL,w.DWORD],HANDLE)
CloseHandle=signature('CloseHandle',[HANDLE],w.BOOL)
VirtualAllocEx=signature('VirtualAllocEx',[HANDLE,PTR,SIZE,w.DWORD,w.DWORD],PTR)
WriteProcessMemory=signature('WriteProcessMemory',[HANDLE,PTR,PTR,SIZE,c.POINTER(SIZE)],w.BOOL)
CreateRemoteThread=signature('CreateRemoteThread',[HANDLE,PTR,SIZE,PTR,PTR,w.DWORD,PTR],HANDLE)
WaitForSingleObject=signature('WaitForSingleObject',[HANDLE,w.DWORD],w.DWORD)
GetExitCodeThread=signature('GetExitCodeThread',[HANDLE,c.POINTER(w.DWORD)],w.BOOL)
ResumeThread=signature('ResumeThread',[HANDLE],w.DWORD)
GetModuleHandleW=signature('GetModuleHandleW',[w.LPCWSTR],HANDLE)
GetProcAddress=signature('GetProcAddress',[HANDLE,c.c_char_p],PTR)
LoadLibraryW=signature('LoadLibraryW',[w.LPCWSTR],HANDLE)
Snapshot=signature('CreateToolhelp32Snapshot',[w.DWORD,w.DWORD],HANDLE)
class ThreadEntry(c.Structure):_fields_=[('size',w.DWORD),('usage',w.DWORD),('id',w.DWORD),('owner',w.DWORD),('priority',w.LONG),('delta',w.LONG),('flags',w.DWORD)]
ThreadFirst=signature('Thread32First',[HANDLE,c.POINTER(ThreadEntry)],w.BOOL)
ThreadNext=signature('Thread32Next',[HANDLE,c.POINTER(ThreadEntry)],w.BOOL)
EnumModules=signature('K32EnumProcessModules',[HANDLE,c.POINTER(HANDLE),w.DWORD,c.POINTER(w.DWORD)],w.BOOL)
ModuleName=signature('K32GetModuleFileNameExW',[HANDLE,HANDLE,w.LPWSTR,w.DWORD],w.DWORD)
class ModuleInfo(c.Structure):_fields_=[('base',PTR),('size',w.DWORD),('entry',PTR)]
ModuleInformation=signature('K32GetModuleInformation',[HANDLE,HANDLE,c.POINTER(ModuleInfo),w.DWORD],w.BOOL)

def checked(value):
 if not value:raise c.WinError(c.get_last_error())
 return value
def ownedThreads(pid):
 snapshot=checked(Snapshot(4,0));entry=ThreadEntry();entry.size=c.sizeof(entry);result=[]
 try:
  ok=ThreadFirst(snapshot,c.byref(entry))
  while ok:
   if entry.owner==pid:result.append(entry.id)
   ok=ThreadNext(snapshot,c.byref(entry))
 finally:CloseHandle(snapshot)
 return result
def modules(process):
 values=(HANDLE*1024)();size=w.DWORD()
 checked(EnumModules(process,values,c.sizeof(values),c.byref(size)))
 assert size.value<=c.sizeof(values),'Module inventory overflow'
 result=[]
 for value in values[:size.value//c.sizeof(HANDLE)]:
  name=c.create_unicode_buffer(4096);info=ModuleInfo()
  if not ModuleName(process,value,name,len(name)):continue
  checked(ModuleInformation(process,value,c.byref(info),c.sizeof(info)))
  result.append(dict(base=info.base,size=info.size,path=name.value))
 return result
def remoteString(process,value):
 data=c.create_unicode_buffer(str(value));address=checked(VirtualAllocEx(process,None,c.sizeof(data),0x3000,4));written=SIZE()
 checked(WriteProcessMemory(process,address,data,c.sizeof(data),c.byref(written)));assert written.value==c.sizeof(data)
 return address
def invoke(process,address,argument):
 thread=checked(CreateRemoteThread(process,None,0,address,argument,0,None))
 try:
  assert WaitForSingleObject(thread,30000)==0,'Diagnostic initialization timed out'
  code=w.DWORD();checked(GetExitCodeThread(thread,c.byref(code)));return code.value
 finally:CloseHandle(thread)

def run(command,output,dll,timeout,residency=False,ready_file=None):
 command=[str(Path(shutil.which(command[0]) or command[0]).resolve()),*command[1:]]
 output=output.resolve();output.mkdir(parents=True,exist_ok=False);dll=dll.resolve()
 inventory={};snapshots=[];snapshotErrors=[];started=time.perf_counter()
 startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
 stdout=(output/'stdout.log').open('w',encoding='utf-8');stderr=(output/'stderr.log').open('w',encoding='utf-8')
 child=subprocess.Popen(command,stdout=stdout,stderr=stderr,creationflags=0 if ready_file else 4,startupinfo=startup)
 process=primary=observer=None;pages=[];observations=[];lastSnapshot=-1
 try:
  process=checked(OpenProcess(0x1fffff,False,child.pid))
  if ready_file:
   while not ready_file.exists():
    assert child.poll() is None,('Owned child exited before readiness',child.returncode)
    assert time.perf_counter()-started<45,'Owned diagnostic startup timed out'
    time.sleep(.01)
  else:
   threads=ownedThreads(child.pid);assert len(threads)==1,'Unexpected threads before diagnostic initialization'
   primary=checked(OpenThread(0x1fffff,False,threads[0]))
  invoke(process,GetProcAddress(GetModuleHandleW('kernel32.dll'),b'LoadLibraryW'),remoteString(process,dll))
  remote=next(row for row in modules(process) if Path(row['path']).resolve()==dll)
  local=checked(LoadLibraryW(str(dll)));entry=checked(GetProcAddress(local,b'AzureTraceStart'))
  result=invoke(process,remote['base']+entry-local,remoteString(process,output/'events.bin'))
  assert result==0,('Diagnostic initialization rejected',result)
  if residency:
   sys.path.insert(0,str(Path.cwd()/'tools'))
   from collect_memory_residency import ResidentObserver
   from run_playable_performance import memory_snapshot
   observer=ResidentObserver(child.pid)
   sample=observer.snapshot();sample['counter']=time.perf_counter();pages.append(sample)
  if primary:assert ResumeThread(primary)==1,'Owned child initial suspension changed'
  while child.poll() is None:
   assert time.perf_counter()-started<timeout,'Owned diagnostic child timed out'
   if observer:
    sample=memory_snapshot(child.pid);sample['seconds']=time.perf_counter()-started;observations.append(sample)
    if sample['seconds']-lastSnapshot>=1:
     snapshot=observer.snapshot();snapshot['counter']=time.perf_counter();pages.append(snapshot);lastSnapshot=sample['seconds']
   try:
    current=modules(process)
    for row in current:inventory[(row['base'],row['size'],row['path'])]=row
    snapshots.append(dict(seconds=time.perf_counter()-started,modules=[row['base'] for row in current]))
   except OSError as error:
    snapshotErrors.append(dict(seconds=time.perf_counter()-started,error=str(error)))
    if child.poll() is None and getattr(error,'winerror',None)!=299:raise
   time.sleep(.05)
 finally:
  if child.poll() is None:
   child.kill();child.wait(timeout=15)
  if primary:CloseHandle(primary)
  if process:CloseHandle(process)
  if observer:observer.close()
  stdout.close();stderr.close()
 if child.returncode or not (output/'events.bin').is_file():
  (output/'failure.json').write_text(json.dumps(dict(diagnosticOnly=True,childExitCode=child.returncode,command=command,modules=list(inventory.values()),snapshotErrors=snapshotErrors),indent=2)+'\n')
  return 1
 raw=(output/'events.bin').read_bytes();header=struct.unpack_from('<8s7Q',raw)
 magic,version,capacity,count,dropped,reentrant,invalid,frequency=header
 assert magic==b'AZNT001\0' and version==1 and len(raw)==64+min(capacity,count)*232
 fields=['qpc','threadId','api','flags','protection','status','process','inputBase','inputSize','base','size','frameCount','published']
 records=[]
 for offset in range(64,len(raw),232):
  values=struct.unpack_from('<29Q',raw,offset);row=dict(zip(fields,values[:13]));row['stack']=list(values[13:13+row['frameCount']]);records.append(row)
 with gzip.open(output/'events.json.gz','wt',encoding='utf-8') as stream:json.dump(records,stream)
 result=dict(diagnosticOnly=True,childPid=child.pid,childExitCode=child.returncode,seconds=time.perf_counter()-started,
  events=count,dropped=dropped,reentrant=reentrant,invalidArguments=invalid,unpublished=sum(not row['published'] for row in records),
  counterFrequency=frequency,completeAllocationCoverage=False,modules=list(inventory.values()),moduleSnapshots=snapshots,snapshotErrors=snapshotErrors,
  hashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (Path(__file__).resolve(),dll,Path(command[0]).resolve())},command=command,
  limits=['Process-local NT virtual allocation calls after explicit initialization only.','Mappings, heap suballocations and kernel allocations are outside coverage.',
  'Injected buffer, hooks and stack capture affect memory and timing.','Periodic module snapshots may miss transient modules.'])
 (output/'summary.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
 if residency:(output/'residency.json').write_text(json.dumps(dict(observations=observations,snapshots=pages),indent=2)+'\n')
 print(json.dumps({key:result[key] for key in ('events','dropped','reentrant','invalidArguments','unpublished','seconds')}),flush=True)
 return int(bool(dropped or invalid or result['unpublished']))
if __name__=='__main__':
 parser=argparse.ArgumentParser(__doc__);parser.add_argument('--output',type=Path,required=True)
 parser.add_argument('--timeout',type=float,default=600);parser.add_argument('--dll',type=Path,default=Path(__file__).parent/'compiled-ninja/AzureNtTrace.dll')
 parser.add_argument('--residency',action='store_true')
 parser.add_argument('--ready-file',type=Path)
 parser.add_argument('command',nargs=argparse.REMAINDER);args=parser.parse_args()
 command=args.command[1:] if args.command[:1]==['--'] else args.command
 assert command and args.timeout>0
 raise SystemExit(run(command,args.output,args.dll,args.timeout,args.residency,args.ready_file))
